/*
 * polygon-ninja.c  —  LUT-based encoder-synchronous polygon turning HAL component
 *
 * Generates an X-axis external offset synchronized to a 4096-CPR spindle
 * encoder, enabling square turning, arbitrary polygon profiles (>=3 sides),
 * and eccentric shapes.
 *
 * The LUT (4096 entries) maps each encoder count directly to a normalized
 * radius.  At runtime the only operation is a table lookup + multiply — no
 * trigonometry in the hot path.  The LUT is (re)built whenever the polygon
 * shape parameters change.
 *
 *   INPUT PINS
 *     polygon-ninja.encoder-count        s32   IN  — raw encoder count (wraps at 4096)
 *     polygon-ninja.enable               bit   IN  — enable module
 *     polygon-ninja.phase-offset         s32   IN  — angular index offset (0..4095)
 *     polygon-ninja.circumscribed-radius float IN  — radius of circumscribed circle (mm)
 *     polygon-ninja.excenter-x           float IN  — X offset of shape center (mm)
 *     polygon-ninja.polygon-sides        s32   IN  — number of polygon sides (>=3)
 *     polygon-ninja.mode                 s32   IN  — 0=absolute, 1=from-min, 2=centered
 *
 *   OUTPUT PINS
 *     polygon-ninja.eoffset-counts       s32   OUT — external offset counts (scaled x1000)
 *     polygon-ninja.enable-out           bit   OUT — goes high after index handshake completes
 *     polygon-ninja.clear-out            bit   OUT — eoffset clear handshake output
 *     polygon-ninja.index-enable         bit   OUT — pulse/request for encoder index reset handshake
 *     polygon-ninja.debug-r-abs-mm       float OUT — instantaneous absolute radius
 *     polygon-ninja.debug-r-min-mm       float OUT — minimum radius for current shape
 *     polygon-ninja.debug-r-max-mm       float OUT — maximum radius for current shape
 *     polygon-ninja.debug-x-target-mm    float OUT — selected mode target radius
 *
 *   REALTIME FUNCTION
 *     polygon-ninja.process — add to servo-thread
 *
 *   EXAMPLE
 *     loadrt polygon-ninja
 *     addf   polygon-ninja.process  servo-thread
 *     net    spindle-count  encoder.0.rawcounts  =>  polygon-ninja.encoder-count
 *     net    eofs           polygon-ninja.eoffset-counts  =>  motion.eoffset-counts
 *     setp   motion.external-offset-enable 1
 *     setp   polygon-ninja.polygon-sides 4
 *     setp   polygon-ninja.circumscribed-radius 25.0
 *
 * Author:  Viola Zsolt
 * License: MIT
 */

#include "rtapi.h"
#include "rtapi_app.h"
#include "rtapi_errno.h"
#include "hal.h"

#include <math.h>
#include <string.h>

#define module_name  "polygon-ninja"

#define MAX_SIDES    64             /* upper bound on polygon sides           */
#define EPS          1e-9f          /* near-zero threshold for intersection   */

typedef enum {
    POLY_STATE_DISABLED = 0,
    POLY_STATE_WAIT_INDEX_CLEAR,
    POLY_STATE_RUNNING,
} polygon_run_state_t;

MODULE_AUTHOR("Viola Zsolt");
MODULE_DESCRIPTION("polygon-ninja: LUT-based encoder-synchronous polygon turning HAL component");
MODULE_LICENSE("MIT");

static int encoder_scale = 4096;
RTAPI_MP_INT(encoder_scale, "LUT size (power of 2, typically 4096 for 4096-CPR encoders)");

/* =========================================================================
 * HAL data structure — one instance, allocated in HAL shared memory
 * ========================================================================= */
typedef struct {

    /* ----- Input pins ---------------------------------------------------- */
    hal_s32_t   *encoder_count;         /* raw encoder count 0..4095          */
    hal_bit_t   *enable;                /* 1 = module active                  */
    hal_s32_t   *phase_offset;          /* angular index shift 0..4095        */
    hal_float_t *circumscribed_radius;  /* circumscribed circle radius (mm)   */
    hal_float_t *excenter_x;            /* polygon X-axis offset (mm)         */
    hal_s32_t   *polygon_sides;         /* number of sides (>=3)              */
    hal_s32_t   *mode;                  /* 0=absolute,1=from-min,2=centered   */

    /* ----- Output pins --------------------------------------------------- */
    hal_s32_t   *eoffset_counts;        /* external offset counts (x1000)     */
    hal_bit_t   *enable_out;            /* enable after index handshake       */
    hal_bit_t   *clear_out;             /* inverse of input enable            */
    hal_bit_t   *index_enable;          /* asserted, then cleared externally  */
    hal_float_t *debug_r_abs_mm;        /* instantaneous absolute radius (mm) */
    hal_float_t *debug_r_min_mm;        /* minimum radius for current shape   */
    hal_float_t *debug_r_max_mm;        /* maximum radius for current shape   */
    hal_float_t *debug_x_target_mm;     /* selected output target in mm       */

    /* ----- Lookup table (written by LUT builder, read by RT path) --------- */
    float *lut;             /* dynamically allocated array of lut_size entries */
    int    lut_size;        /* configured LUT size (must be power of 2)        */
    int    lut_mask;        /* lut_size - 1 for bitmask indexing               */

    /* ----- Polygon memory ------------------------------------------------- */
    float poly_x[MAX_SIDES + 1];
    float poly_y[MAX_SIDES + 1];
    int   poly_vertices;
    hal_bit_t polygon_error_latched;
    hal_bit_t polygon_valid;

    /* ----- Change detection (trigger LUT rebuild on shape change) --------- */
    int   prev_sides;
    float prev_ex_x;
    float prev_circ_radius;
    float lut_min_norm;
    float lut_max_norm;
    float lut_mean_norm;
    polygon_run_state_t run_state;
    hal_bit_t prev_enable;

} polygon_data_t;

static int            comp_id  = -1;
static polygon_data_t *hal_data = NULL;

/* =========================================================================
 * Forward declarations
 * ========================================================================= */
static void polygon_process(void *arg, long period);
static int  build_lut(polygon_data_t *d, int sides, float ex_x, float circ_radius);
static void build_regular_polygon_memory(polygon_data_t *d, int sides);
static int  build_lut_from_polygon_memory(polygon_data_t *d, float ex_x, float circ_radius);
static int  validate_polygon_memory(polygon_data_t *d);
static int  intersect_ray_segment(float dx, float dy,
                                  float x1, float y1,
                                  float x2, float y2,
                                  float *t_hit);
static int  is_power_of_2(int n);

/* =========================================================================
 * Utility: check if an integer is a power of 2
 * ========================================================================= */
static int is_power_of_2(int n)
{
    return n > 0 && (n & (n - 1)) == 0;
}

/* =========================================================================
 * Geometry helper: intersection between a ray from origin and a line segment
 *
 * Ray:     P = t * (dx, dy), t >= 0
 * Segment: Q = (x1, y1) + u * ((x2-x1), (y2-y1)), 0 <= u <= 1
 *
 * Returns 1 on valid hit and writes the ray distance to *t_hit.
 * Returns 0 for parallel/no-hit cases.
 * ========================================================================= */
static int intersect_ray_segment(float dx, float dy,
                                 float x1, float y1,
                                 float x2, float y2,
                                 float *t_hit)
{
    float sx = x2 - x1;
    float sy = y2 - y1;
    float det = dx * sy - dy * sx;

    if (fabsf(det) < EPS) {
        return 0;
    }

    float t = (x1 * sy - y1 * sx) / det;
    float u = (x1 * dy - y1 * dx) / det;

    if (t > EPS && u >= -EPS && u <= 1.0f + EPS) {
        *t_hit = t;
        return 1;
    }

    return 0;
}

/* =========================================================================
 * Polygon memory builder
 *
 * Fills the persistent polygon coordinate memory with a regular N-gon on the
 * unit circle. The LUT builder then works only from this coordinate memory,
 * which makes it straightforward to support custom polygon sources later.
 * ========================================================================= */
static void build_regular_polygon_memory(polygon_data_t *d, int sides)
{
    int k;
    float a;

    if (sides < 3) {
        sides = 3;
    }
    if (sides > MAX_SIDES) {
        sides = MAX_SIDES;
    }

    d->poly_vertices = sides + 1;

    for (k = 0; k < sides; k++) {
        a = 2.0f * (float)M_PI * k / (float)sides;
        d->poly_x[k] = cosf(a);
        d->poly_y[k] = sinf(a);
    }

    d->poly_x[sides] = d->poly_x[0];
    d->poly_y[sides] = d->poly_y[0];
}

/* =========================================================================
 * Polygon validation
 *
 * A valid polygon memory must contain at least 3 unique vertices plus the
 * closing point, and the final point must match the first point.
 * ========================================================================= */
static int validate_polygon_memory(polygon_data_t *d)
{
    int k;
    float dx, dy;

    if (d->poly_vertices < 4) {
        return 0;
    }

    dx = d->poly_x[d->poly_vertices - 1] - d->poly_x[0];
    dy = d->poly_y[d->poly_vertices - 1] - d->poly_y[0];
    if ((dx * dx + dy * dy) > (EPS * EPS)) {
        return 0;
    }

    for (k = 0; k < d->poly_vertices - 1; k++) {
        dx = d->poly_x[k + 1] - d->poly_x[k];
        dy = d->poly_y[k + 1] - d->poly_y[k];
        if ((dx * dx + dy * dy) <= (EPS * EPS)) {
            return 0;
        }
    }

    return 1;
}

/* =========================================================================
 * LUT generation from polygon memory
 *
 * Uses the currently stored polygon coordinate memory and computes the radial
 * intersection by explicit ray/segment line-line intersection.
 * ========================================================================= */
static int build_lut_from_polygon_memory(polygon_data_t *d, float ex_x, float circ_radius)
{
    int i, k;
    float theta, dx, dy;
    float x1, y1, x2, y2;
    float t_hit, t_min;
    float r, px, py, ex_x_norm;
    float r_min = 1e30f;
    float r_max = -1e30f;
    double r_sum = 0.0;

    if (!validate_polygon_memory(d)) {
        if (!d->polygon_error_latched) {
            rtapi_print_msg(RTAPI_MSG_ERR,
                module_name ": polygon memory is not closed or contains degenerate edges\n");
            d->polygon_error_latched = 1;
        }
        d->polygon_valid = 0;
        return -EINVAL;
    }

    d->polygon_error_latched = 0;
    d->polygon_valid = 1;

    if (fabsf(circ_radius) > EPS) {
        ex_x_norm = ex_x / circ_radius;
    } else {
        ex_x_norm = 0.0f;
    }

    for (i = 0; i < d->lut_size; i++) {
        theta = 2.0f * (float)M_PI * i / (float)d->lut_size;
        dx = cosf(theta);
        dy = sinf(theta);
        t_min = 1e30f;

        for (k = 0; k < d->poly_vertices - 1; k++) {
            x1 = d->poly_x[k];
            y1 = d->poly_y[k];
            x2 = d->poly_x[k + 1];
            y2 = d->poly_y[k + 1];

            if (intersect_ray_segment(dx, dy, x1, y1, x2, y2, &t_hit)) {
                if (t_hit < t_min) {
                    t_min = t_hit;
                }
            }
        }

        r = (t_min < 1e29f) ? t_min : 1.0f;

        px = r * dx + ex_x_norm;
        py = r * dy;
        d->lut[i] = sqrtf(px * px + py * py);

        if (d->lut[i] < r_min) {
            r_min = d->lut[i];
        }
        if (d->lut[i] > r_max) {
            r_max = d->lut[i];
        }
        r_sum += (double)d->lut[i];
    }

    d->lut_min_norm = r_min;
    d->lut_max_norm = r_max;
    d->lut_mean_norm = (float)(r_sum / (double)d->lut_size);
    return 0;
}

/* =========================================================================
 * LUT generation
 *
 * Builds a LUT that maps encoder index → normalized radius of the
 * requested polygon.
 *
 * Algorithm:
 *   1. Generate a regular N-sided polygon inscribed in the unit circle.
 *   2. For each LUT entry i:
 *        theta = 2*pi*i / lut_size
 *        Cast ray (cos(theta), sin(theta)) from the origin.
 *        Find the closest intersection with any polygon edge.
 *        Store the resulting radius.
 *
 * This function uses sin/cos — NOT safe for hard-RT kernels when called
 * from the servo thread.  On PREEMPT-RT (the target platform) these are
 * plain libc calls available in the HAL shared-object context.
 * LUT rebuilds are rare (only on shape-parameter change) so any brief
 * latency spike is acceptable during a parameter transition.
 * ========================================================================= */
static int build_lut(polygon_data_t *d, int sides, float ex_x, float circ_radius)
{
    if (sides < 3)        sides = 3;
    if (sides > MAX_SIDES) sides = MAX_SIDES;

    build_regular_polygon_memory(d, sides);
    if (build_lut_from_polygon_memory(d, ex_x, circ_radius) < 0) {
        d->prev_sides = sides;
        d->prev_ex_x = ex_x;
        d->prev_circ_radius = circ_radius;
        return -EINVAL;
    }

    /* Save for change detection */
    d->prev_sides = sides;
    d->prev_ex_x  = ex_x;
    d->prev_circ_radius = circ_radius;
    return 0;
}

/* =========================================================================
 * Realtime process function — O(1), no trig, no allocation
 * ========================================================================= */
static void polygon_process(void *arg, long period)
{
    polygon_data_t *d = (polygon_data_t *)arg;
    (void)period;
    hal_bit_t enable_now = (hal_bit_t)(*d->enable != 0);
    hal_bit_t next_prev_enable = enable_now;

    int   sides       = (int)*d->polygon_sides;
    float ex_x        = (float)*d->excenter_x;
    float circ_radius = (float)*d->circumscribed_radius;
    int mode = (int)*d->mode;

    if (sides < 3)        sides = 3;
    if (sides > MAX_SIDES) sides = MAX_SIDES;

    /* Rebuild LUT only when shape parameters change */
    if (sides != d->prev_sides ||
        ex_x  != d->prev_ex_x ||
        circ_radius != d->prev_circ_radius)
    {
        build_lut(d, sides, ex_x, circ_radius);
    }

    if (!d->polygon_valid) {
        *d->eoffset_counts = 0;
        *d->enable_out = 0;
        *d->clear_out = 1;
        *d->index_enable = 0;
        *d->debug_r_abs_mm = 0.0;
        *d->debug_x_target_mm = 0.0;
        d->run_state = POLY_STATE_DISABLED;
        next_prev_enable = 0;
    } else if (!enable_now) {
        /* Disabled path — zero output and reset handshake */
        *d->eoffset_counts = 0;
        *d->enable_out = 0;
        *d->clear_out = 1;
        *d->index_enable = 0;
        *d->debug_r_abs_mm = 0.0;
        *d->debug_x_target_mm = 0.0;
        *d->debug_r_min_mm = d->lut_min_norm * circ_radius;
        *d->debug_r_max_mm = d->lut_max_norm * circ_radius;
        d->run_state = POLY_STATE_DISABLED;
        next_prev_enable = 0;
    } else {
        *d->clear_out = 0;

        /* On enable edge: request index handshake first, but do not start output yet. */
        if (!d->prev_enable) {
            *d->index_enable = 1;
            *d->enable_out = 0;
            *d->eoffset_counts = 0;
            d->run_state = POLY_STATE_WAIT_INDEX_CLEAR;
        }

        switch (d->run_state) {
            case POLY_STATE_WAIT_INDEX_CLEAR:
                /* Another component is expected to clear index-enable after servicing it. */
                if (!*d->index_enable) {
                    d->run_state = POLY_STATE_RUNNING;
                    *d->enable_out = 1;
                } else {
                    *d->enable_out = 0;
                    *d->eoffset_counts = 0;
                    next_prev_enable = 1;
                }
                break;
            case POLY_STATE_RUNNING:
                break;
            case POLY_STATE_DISABLED:
            default:
                /* Safety fallback: if enabled but state is disabled, restart handshake. */
                *d->index_enable = 1;
                *d->enable_out = 0;
                *d->eoffset_counts = 0;
                d->run_state = POLY_STATE_WAIT_INDEX_CLEAR;
                next_prev_enable = 1;
                break;
        }

        if (d->run_state == POLY_STATE_RUNNING) {
            /* Encoder count → LUT index (bitmask wrap, no modulo) */
            int idx = ((int)*d->encoder_count + (int)*d->phase_offset) & d->lut_mask;

            /* Scale normalized radius by circumscribed-radius pin */
            float r_abs_mm = d->lut[idx] * circ_radius;
            float r_min_mm = d->lut_min_norm * circ_radius;
            float r_max_mm = d->lut_max_norm * circ_radius;
            float r_mean_mm = d->lut_mean_norm * circ_radius;
            float x_target_mm;

            if (mode == 1) {
                x_target_mm = r_abs_mm - r_min_mm;     /* starts from zero at flats */
            } else if (mode == 2) {
                x_target_mm = r_abs_mm - r_mean_mm;    /* centered around zero */
            } else {
                x_target_mm = r_abs_mm;                /* absolute radius */
            }

            *d->debug_r_abs_mm = r_abs_mm;
            *d->debug_r_min_mm = r_min_mm;
            *d->debug_r_max_mm = r_max_mm;
            *d->debug_x_target_mm = x_target_mm;

            *d->eoffset_counts = (hal_s32_t)lroundf(x_target_mm * 1000.0f);
            *d->enable_out = 1;
        }
    }

    d->prev_enable = next_prev_enable;
}

/* =========================================================================
 * Module init / exit
 * ========================================================================= */
int rtapi_app_main(void)
{
    int retval;

    comp_id = hal_init(module_name);
    if (comp_id < 0) {
        rtapi_print_msg(RTAPI_MSG_ERR,
            module_name ": hal_init failed: %d\n", comp_id);
        return comp_id;
    }

    hal_data = (polygon_data_t *)hal_malloc(sizeof(polygon_data_t));
    if (!hal_data) {
        rtapi_print_msg(RTAPI_MSG_ERR, module_name ": hal_malloc failed\n");
        hal_exit(comp_id);
        return -ENOMEM;
    }
    memset(hal_data, 0, sizeof(*hal_data));

    /* Validate and configure LUT size */
    if (!is_power_of_2(encoder_scale)) {
        rtapi_print_msg(RTAPI_MSG_ERR,
            module_name ": encoder-scale must be a power of 2 (got %d)\n",
            encoder_scale);
        hal_exit(comp_id);
        return -EINVAL;
    }
    hal_data->lut_size = encoder_scale;
    hal_data->lut_mask = encoder_scale - 1;

    /* Allocate LUT in HAL shared memory */
    hal_data->lut = (float *)hal_malloc(encoder_scale * sizeof(float));
    if (!hal_data->lut) {
        rtapi_print_msg(RTAPI_MSG_ERR,
            module_name ": hal_malloc for LUT (%d entries) failed\n",
            encoder_scale);
        hal_exit(comp_id);
        return -ENOMEM;
    }

/* Convenience macros for pin export */
#define PIN_S32(ptr, dir, name) \
    do { retval = hal_pin_s32_newf((dir), (ptr), comp_id, module_name "." name); \
         if (retval < 0) goto err; } while (0)
#define PIN_BIT(ptr, dir, name) \
    do { retval = hal_pin_bit_newf((dir), (ptr), comp_id, module_name "." name); \
         if (retval < 0) goto err; } while (0)
#define PIN_FLOAT(ptr, dir, name) \
    do { retval = hal_pin_float_newf((dir), (ptr), comp_id, module_name "." name); \
         if (retval < 0) goto err; } while (0)

    /* Input pins */
    PIN_S32  (&hal_data->encoder_count,        HAL_IN,  "encoder-count");
    PIN_BIT  (&hal_data->enable,               HAL_IN,  "enable");
    PIN_S32  (&hal_data->phase_offset,         HAL_IN,  "phase-offset");
    PIN_FLOAT(&hal_data->circumscribed_radius, HAL_IN,  "circumscribed-radius");
    PIN_FLOAT(&hal_data->excenter_x,           HAL_IN,  "excenter-x");
    PIN_S32  (&hal_data->polygon_sides,        HAL_IN,  "polygon-sides");
    PIN_S32  (&hal_data->mode,                 HAL_IN,  "mode");

    /* Output pins */
    PIN_S32  (&hal_data->eoffset_counts,       HAL_OUT, "eoffset-counts");
    PIN_BIT  (&hal_data->enable_out,           HAL_OUT, "enable-out");
    PIN_BIT  (&hal_data->clear_out,            HAL_OUT, "clear-out");
    PIN_BIT  (&hal_data->index_enable,         HAL_IO, "index-enable");
    PIN_FLOAT(&hal_data->debug_r_abs_mm,       HAL_OUT, "debug-r-abs-mm");
    PIN_FLOAT(&hal_data->debug_r_min_mm,       HAL_OUT, "debug-r-min-mm");
    PIN_FLOAT(&hal_data->debug_r_max_mm,       HAL_OUT, "debug-r-max-mm");
    PIN_FLOAT(&hal_data->debug_x_target_mm,    HAL_OUT, "debug-x-target-mm");

#undef PIN_S32
#undef PIN_BIT
#undef PIN_FLOAT

    retval = hal_export_funct(module_name ".process", polygon_process,
                              hal_data, 1, 0, comp_id);
    if (retval < 0) {
        rtapi_print_msg(RTAPI_MSG_ERR,
            module_name ": hal_export_funct failed: %d\n", retval);
        goto err;
    }

    /* Set default pin values and build initial LUT (4 sides = square) */
    *hal_data->polygon_sides        = 4;
    *hal_data->circumscribed_radius = 20.0;
    *hal_data->excenter_x           = 0.0;
    *hal_data->phase_offset         = 0;
    *hal_data->mode                 = 0;
    *hal_data->eoffset_counts       = 0;
    *hal_data->enable_out           = 0;
    *hal_data->clear_out            = 1;
    *hal_data->index_enable         = 0;
    *hal_data->debug_r_abs_mm       = 0.0;
    *hal_data->debug_r_min_mm       = 0.0;
    *hal_data->debug_r_max_mm       = 0.0;
    *hal_data->debug_x_target_mm    = 0.0;
    hal_data->polygon_error_latched = 0;
    hal_data->polygon_valid         = 1;
    hal_data->run_state            = POLY_STATE_DISABLED;
    hal_data->prev_enable           = 0;

    build_lut(hal_data, 4, 0.0f, *hal_data->circumscribed_radius);

    hal_ready(comp_id);

    rtapi_print_msg(RTAPI_MSG_INFO,
        module_name ": loaded — LUT ready (size: %d entries, default: 4 sides)\n",
        encoder_scale);
    return 0;

err:
    rtapi_print_msg(RTAPI_MSG_ERR,
        module_name ": pin export failed: %d\n", retval);
    hal_exit(comp_id);
    return retval;
}

void rtapi_app_exit(void)
{
    hal_exit(comp_id);
}
