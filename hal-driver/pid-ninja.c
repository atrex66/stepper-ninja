/*
 * pid-ninja.c  —  Realtime PID controller HAL component
 *
 * Supports up to MAX_CHAN independent PID instances, each exposing:
 *
 *   INPUT PINS
 *     <name>.<N>.command          float IN  — setpoint / reference
 *     <name>.<N>.feedback         float IN  — process variable (measured)
 *     <name>.<N>.enable           bit   IN  — 1 = controller active
 *
 *   OUTPUT PINS
 *     <name>.<N>.output           float OUT — control output
 *     <name>.<N>.error            float OUT — raw error (command - feedback)
 *     <name>.<N>.error-I          float OUT — accumulated integral of error
 *     <name>.<N>.error-D          float OUT — filtered derivative of error
 *     <name>.<N>.saturated        bit   OUT — 1 = output is clamped
 *
 *   PARAMETERS  (set via:  setp <name>.<N>.<param> <value>)
 *     Pgain               proportional gain                 [default 1.0]
 *     Igain               integral gain                     [default 0.0]
 *     Dgain               derivative gain                   [default 0.0]
 *     FF0                 0th-order feed-forward            [default 0.0]
 *     FF1                 1st-order feed-forward (velocity) [default 0.0]
 *     FF2                 2nd-order feed-forward (accel)    [default 0.0]
 *     bias                constant output offset            [default 0.0]
 *     max-output          symmetric output clamp; 0=off     [default 0.0]
 *     max-error           symmetric error clamp;  0=off     [default 0.0]
 *     max-error-I         integrator anti-windup; 0=off     [default 0.0]
 *     max-error-D         derivative error clamp; 0=off     [default 0.0]
 *     deadband            error deadband around zero        [default 0.0]
 *     deriv-filter-weight low-pass weight for D [0..1]      [default 0.0]
 *
 *   REALTIME FUNCTION
 *     <name>.<N>.process — add to a fast servo thread
 *
 *   MODULE PARAMETER
 *     instances=<N>   number of PID instances to create (1–8, default 1)
 *
 *   EXAMPLE HAL USAGE
 *     loadrt pid-ninja instances=3
 *     addf   pid-ninja.0.process  servo-thread
 *     net    x-pos-cmd   pid-ninja.0.command
 *     net    x-pos-fb    pid-ninja.0.feedback
 *     net    x-output    pid-ninja.0.output
 *     setp   pid-ninja.0.Pgain  100.0
 *     setp   pid-ninja.0.Igain    5.0
 *     setp   pid-ninja.0.Dgain    2.0
 *     setp   pid-ninja.0.max-output 10.0
 */

#include "rtapi.h"              /* RTAPI realtime OS API */
#include "rtapi_app.h"          /* RTAPI realtime module decls */
#include "rtapi_errno.h"        /* EINVAL etc */
#include "hal.h"                /* HAL public API decls */

#include <string.h>
#include <stdio.h>

#define module_name "pid-ninja"
#define MAX_CHAN    8

/* Module information */
MODULE_AUTHOR("Viola Zsolt");
MODULE_DESCRIPTION("pid-ninja: realtime PID controller HAL component");
MODULE_LICENSE("MIT");

static int instances = 1;
RTAPI_MP_INT(instances, "Number of PID controller instances (1-8)");

/* =========================================================================
 * HAL data structure — one per channel, allocated in HAL shared memory
 * ========================================================================= */
typedef struct {

    /* ----- Input pins ---------------------------------------------------- */
    hal_float_t *command;           /* setpoint / reference                  */
    hal_float_t *feedback;          /* process variable (measured)           */
    hal_bit_t   *enable;            /* 1 = controller active                 */

    /* ----- Output pins --------------------------------------------------- */
    hal_float_t *output;            /* control output                        */
    hal_float_t *error;             /* raw error = command - feedback        */
    hal_float_t *error_i;           /* accumulated integral of error         */
    hal_float_t *error_d;           /* filtered derivative of error          */
    hal_bit_t   *saturated;         /* 1 = output is clamped                 */

    /* ----- Tuning parameters (set via  setp) ----------------------------- */
    hal_float_t  Pgain;             /* proportional gain                     */
    hal_float_t  Igain;             /* integral gain                         */
    hal_float_t  Dgain;             /* derivative gain                       */
    hal_float_t  FF0;               /* 0th-order feed-forward                */
    hal_float_t  FF1;               /* 1st-order feed-forward (velocity)     */
    hal_float_t  FF2;               /* 2nd-order feed-forward (acceleration) */
    hal_float_t  bias;              /* constant output offset                */
    hal_float_t  maxoutput;         /* symmetric output clamp; 0 = disabled  */
    hal_float_t  maxerror;          /* symmetric error clamp;  0 = disabled  */
    hal_float_t  maxerrorI;         /* integrator anti-windup; 0 = disabled  */
    hal_float_t  maxerrorD;         /* derivative error clamp; 0 = disabled  */
    hal_float_t  deadband;          /* error deadband around zero            */
    hal_float_t  deriv_filter_weight; /* D low-pass weight [0..1]; 0 = off   */

    /* ----- Internal controller state (not HAL-visible) ------------------- */
    double prev_error;
    double prev_command;
    double prev_prev_command;
    double integral;
    double deriv_filtered;
    int    first_cycle;

} pid_data_t;

static int         comp_id  = -1;
static pid_data_t *hal_data = NULL;

/* =========================================================================
 * Forward declarations
 * ========================================================================= */
static void pid_process        (void *arg, long period);
static int  create_pins_params (int idx);
static void pin_float (hal_float_t **p, int dir, const char *nm, int idx);
static void pin_bit   (hal_bit_t   **p, int dir, const char *nm, int idx);
static void param_float(hal_float_t *p, int rw,  const char *nm, int idx);

/* =========================================================================
 * Realtime process function
 * ========================================================================= */
static void pid_process(void *arg, long period) {
    pid_data_t *d = (pid_data_t *)arg;

    const double dt     = (double)period * 1e-9;        /* ns → s          */
    const double inv_dt = (dt > 1e-12) ? (1.0 / dt) : 0.0;

    /* ---- Disabled: reset state, emit zero output ----------------------- */
    if (!*d->enable) {
        d->integral          = 0.0;
        d->prev_error        = 0.0;
        d->deriv_filtered    = 0.0;
        d->prev_command      = *d->command;
        d->prev_prev_command = *d->command;
        d->first_cycle       = 1;
        *d->output    = 0.0;
        *d->error     = 0.0;
        *d->error_i   = 0.0;
        *d->error_d   = 0.0;
        *d->saturated = 0;
        return;
    }

    const double cmd = *d->command;
    const double fb  = *d->feedback;

    /* ---- Error with deadband and clamping ------------------------------ */
    double e = cmd - fb;

    const double db = (double)d->deadband;
    if (db > 0.0) {
        if      (e >  db) e -= db;
        else if (e < -db) e += db;
        else              e  = 0.0;
    }

    const double me = (double)d->maxerror;
    if (me > 0.0) {
        if (e >  me) e =  me;
        if (e < -me) e = -me;
    }

    /* ---- Proportional term --------------------------------------------- */
    const double P = (double)d->Pgain * e;

    /* ---- Integral term with anti-windup --------------------------------- */
    if (!d->first_cycle)
        d->integral += e * dt;

    const double mi = (double)d->maxerrorI;
    if (mi > 0.0) {
        if (d->integral >  mi) d->integral =  mi;
        if (d->integral < -mi) d->integral = -mi;
    }
    const double I = (double)d->Igain * d->integral;

    /* ---- Derivative term (on error, low-pass filtered) ----------------- */
    double de_dt = 0.0;
    if (!d->first_cycle)
        de_dt = (e - d->prev_error) * inv_dt;

    /* IIR low-pass:  y[n] = w*y[n-1] + (1-w)*x[n],  w in [0, 1) */
    const double w = (double)d->deriv_filter_weight;
    const double wc = (w > 0.0 && w < 1.0) ? w : 0.0;
    d->deriv_filtered = wc * d->deriv_filtered + (1.0 - wc) * de_dt;

    double de_clamped = d->deriv_filtered;
    const double md = (double)d->maxerrorD;
    if (md > 0.0) {
        if (de_clamped >  md) de_clamped =  md;
        if (de_clamped < -md) de_clamped = -md;
    }
    const double D = (double)d->Dgain * de_clamped;

    /* ---- Feed-forward terms -------------------------------------------- */
    double dcmd_dt   = 0.0;
    double d2cmd_dt2 = 0.0;
    if (!d->first_cycle) {
        dcmd_dt   = (cmd - d->prev_command) * inv_dt;
        d2cmd_dt2 = (cmd - 2.0 * d->prev_command + d->prev_prev_command)
                    * inv_dt * inv_dt;
    }
    const double FF = (double)d->FF0 * cmd
                    + (double)d->FF1 * dcmd_dt
                    + (double)d->FF2 * d2cmd_dt2;

    /* ---- Output with symmetric clamping --------------------------------- */
    double out = P + I + D + FF + (double)d->bias;

    const double mo = (double)d->maxoutput;
    int sat = 0;
    if (mo > 0.0) {
        if (out >  mo) { out =  mo; sat = 1; }
        if (out < -mo) { out = -mo; sat = 1; }
    }

    /* Back-calculate anti-windup: undo integration when output is clamped */
    if (sat && d->Igain != 0.0)
        d->integral -= e * dt;

    /* ---- Advance internal state ---------------------------------------- */
    d->prev_error        = e;
    d->prev_prev_command = d->prev_command;
    d->prev_command      = cmd;
    d->first_cycle       = 0;

    /* ---- Write HAL outputs --------------------------------------------- */
    *d->output    = (hal_float_t)out;
    *d->error     = (hal_float_t)(cmd - fb);   /* report pre-deadband error */
    *d->error_i   = (hal_float_t)d->integral;
    *d->error_d   = (hal_float_t)de_clamped;
    *d->saturated = (hal_bit_t)sat;
}

/* =========================================================================
 * HAL pin / parameter creation helpers
 * ========================================================================= */
static void pin_float(hal_float_t **p, int dir, const char *nm, int idx) {
    char name[80] = {0};
    int r;
    snprintf(name, sizeof(name), module_name ".%d.%s", idx, nm);
    r = hal_pin_float_newf(dir, p, comp_id, "%s", name);
    if (r < 0) {
        rtapi_print_msg(RTAPI_MSG_ERR,
            module_name ": ERROR: pin %s export failed: %i\n", name, r);
        hal_exit(comp_id);
    }
}

static void pin_bit(hal_bit_t **p, int dir, const char *nm, int idx) {
    char name[80] = {0};
    int r;
    snprintf(name, sizeof(name), module_name ".%d.%s", idx, nm);
    r = hal_pin_bit_newf(dir, p, comp_id, "%s", name);
    if (r < 0) {
        rtapi_print_msg(RTAPI_MSG_ERR,
            module_name ": ERROR: pin %s export failed: %i\n", name, r);
        hal_exit(comp_id);
    }
}

static void param_float(hal_float_t *p, int rw, const char *nm, int idx) {
    char name[80] = {0};
    int r;
    snprintf(name, sizeof(name), module_name ".%d.%s", idx, nm);
    r = hal_param_float_newf(rw, p, comp_id, "%s", name);
    if (r < 0) {
        rtapi_print_msg(RTAPI_MSG_ERR,
            module_name ": ERROR: param %s export failed: %i\n", name, r);
        hal_exit(comp_id);
    }
}

/* =========================================================================
 * Create all pins and parameters for one instance, then set safe defaults
 * ========================================================================= */
static int create_pins_params(int idx) {
    pid_data_t *d = &hal_data[idx];

    /* Input pins */
    pin_float(&d->command,  HAL_IN,  "command",  idx);
    pin_float(&d->feedback, HAL_IN,  "feedback", idx);
    pin_bit  (&d->enable,   HAL_IN,  "enable",   idx);

    /* Output pins */
    pin_float(&d->output,    HAL_OUT, "output",    idx);
    pin_float(&d->error,     HAL_OUT, "error",     idx);
    pin_float(&d->error_i,   HAL_OUT, "error-I",   idx);
    pin_float(&d->error_d,   HAL_OUT, "error-D",   idx);
    pin_bit  (&d->saturated, HAL_OUT, "saturated", idx);

    /* Tuning parameters */
    param_float(&d->Pgain,              HAL_RW, "Pgain",               idx);
    param_float(&d->Igain,              HAL_RW, "Igain",               idx);
    param_float(&d->Dgain,              HAL_RW, "Dgain",               idx);
    param_float(&d->FF0,                HAL_RW, "FF0",                 idx);
    param_float(&d->FF1,                HAL_RW, "FF1",                 idx);
    param_float(&d->FF2,                HAL_RW, "FF2",                 idx);
    param_float(&d->bias,               HAL_RW, "bias",                idx);
    param_float(&d->maxoutput,          HAL_RW, "max-output",          idx);
    param_float(&d->maxerror,           HAL_RW, "max-error",           idx);
    param_float(&d->maxerrorI,          HAL_RW, "max-error-I",         idx);
    param_float(&d->maxerrorD,          HAL_RW, "max-error-D",         idx);
    param_float(&d->deadband,           HAL_RW, "deadband",            idx);
    param_float(&d->deriv_filter_weight,HAL_RW, "deriv-filter-weight", idx);

    /* Safe defaults */
    d->Pgain               = 1.0;
    d->Igain               = 0.0;
    d->Dgain               = 0.0;
    d->FF0                 = 0.0;
    d->FF1                 = 0.0;
    d->FF2                 = 0.0;
    d->bias                = 0.0;
    d->maxoutput           = 0.0;   /* 0 = unlimited */
    d->maxerror            = 0.0;   /* 0 = unlimited */
    d->maxerrorI           = 0.0;   /* 0 = unlimited */
    d->maxerrorD           = 0.0;   /* 0 = unlimited */
    d->deadband            = 0.0;
    d->deriv_filter_weight = 0.0;   /* 0 = unfiltered */

    /* Init internal state */
    d->prev_error        = 0.0;
    d->prev_command      = 0.0;
    d->prev_prev_command = 0.0;
    d->integral          = 0.0;
    d->deriv_filtered    = 0.0;
    d->first_cycle       = 1;

    return 0;
}

/* =========================================================================
 * Module init / exit
 * ========================================================================= */
int rtapi_app_main(void) {
    int i, r;

    rtapi_set_msg_level(RTAPI_MSG_INFO);

    if (instances < 1 || instances > MAX_CHAN) {
        rtapi_print_msg(RTAPI_MSG_ERR,
            module_name ": instances must be 1..%d (got %d)\n",
            MAX_CHAN, instances);
        return -EINVAL;
    }

    comp_id = hal_init(module_name);
    if (comp_id < 0) {
        rtapi_print_msg(RTAPI_MSG_ERR,
            module_name ": hal_init failed: %d\n", comp_id);
        return comp_id;
    }

    hal_data = (pid_data_t *)hal_malloc(instances * sizeof(pid_data_t));
    if (!hal_data) {
        rtapi_print_msg(RTAPI_MSG_ERR, module_name ": hal_malloc failed\n");
        hal_exit(comp_id);
        return -ENOMEM;
    }
    memset(hal_data, 0, instances * sizeof(pid_data_t));

    for (i = 0; i < instances; i++) {
        r = create_pins_params(i);
        if (r < 0) {
            hal_exit(comp_id);
            return r;
        }

        char fname[80] = {0};
        snprintf(fname, sizeof(fname), module_name ".%d.process", i);
        r = hal_export_funct(fname, pid_process, &hal_data[i], 1, 1, comp_id);
        if (r < 0) {
            rtapi_print_msg(RTAPI_MSG_ERR,
                module_name ": hal_export_funct '%s' failed: %d\n", fname, r);
            hal_exit(comp_id);
            return r;
        }

        rtapi_print_msg(RTAPI_MSG_INFO,
            module_name ": instance %d ready\n", i);
    }

    r = hal_ready(comp_id);
    if (r < 0) {
        rtapi_print_msg(RTAPI_MSG_ERR,
            module_name ": hal_ready failed: %d\n", r);
        hal_exit(comp_id);
        return r;
    }

    rtapi_print_msg(RTAPI_MSG_INFO,
        module_name ": %d instance(s) loaded and ready.\n", instances);
    return 0;
}

void rtapi_app_exit(void) {
    rtapi_print_msg(RTAPI_MSG_INFO, module_name ": unloading.\n");
    hal_exit(comp_id);
}
