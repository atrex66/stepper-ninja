#include "rtapi.h"
#include "rtapi_app.h"
#include "rtapi_errno.h"
#include "hal.h"
#include "math.h"

#include <stdio.h>
#include <string.h>

#define module_name "spindle-orient"

/*
 * Preconfigured number of blocking inputs.
 * Increase/decrease and rebuild the module as needed.
 */
#define BLOCK_INPUT_COUNT 8

/* module information */
MODULE_AUTHOR("Viola Zsolt");
MODULE_DESCRIPTION(module_name " HAL driver");
MODULE_LICENSE("MIT");

#if (BLOCK_INPUT_COUNT <= 0)
#error "BLOCK_INPUT_COUNT must be greater than 0"
#endif

typedef struct {
    hal_bit_t *orient_in;
    hal_float_t *orient_angle;
    hal_float_t *locking_deviation;
    hal_bit_t *spindle_on_in;
    hal_bit_t *spindle_x_index_enable;
    hal_bit_t *spindle_index_enable;
    hal_bit_t *spindle_index_out;
    hal_bit_t *spindle_not_locked;
    hal_float_t *spindle_speed_in;
    hal_float_t *spindle_pid_output_in;
    hal_float_t *spindle_pid_command_out;
    hal_float_t *spindle_command_out;
    hal_bit_t *spindle_enable_out;
    hal_float_t *spindle_pos;
    hal_s32_t *spindle_raw_count;
    hal_float_t *spindle_angle;
    hal_bit_t *spindle_lock_out;
    hal_float_t *angle_deviation;
    hal_bit_t *is_oriented;
    hal_float_t *Kp;
    hal_float_t *Ki;
    hal_float_t *Kd;
    hal_float_t *out_min;
    hal_float_t *out_max;
    hal_float_t *ff0;
    hal_float_t *ff1;
    hal_float_t *ff2;
    hal_bit_t *pid_reset;
    hal_bit_t *chuck_ready;
    hal_bit_t *tailstock_ready;
    hal_bit_t *key_switch;
    hal_bit_t *door_closed;
    hal_bit_t *error_trigg;
    hal_bit_t *spindle_slow_speed;
    hal_u32_t *pid_cycles;
} module_data_t;

typedef struct {
    float Kp;
    float Ki;
    float Kd;

    float integral;
    float prev_error;

    float output_min;
    float output_max;

    float ff0;
    float ff1;
    float ff2;

    float prev_setpoint;
    float prev_velocity;

} PID;

void PID_Init(PID *pid, float Kp, float Ki, float Kd, float out_min, float out_max, float ff0, float ff1, float ff2){
    pid->Kp = Kp;
    pid->Ki = Ki;
    pid->Kd = Kd;

    pid->integral = 0.0f;
    pid->prev_error = 0.0f;

    pid->output_min = out_min;
    pid->output_max = out_max;

    pid->ff0 = ff0;
    pid->ff1 = ff1;
    pid->ff2 = ff2;

    pid->prev_setpoint = 0.0f;
    pid->prev_velocity = 0.0f;
}

float PID_Compute(PID *pid, float setpoint, float measurement, float dt){
    float error = setpoint - measurement;
    float P = pid->Kp * error;
    pid->integral += error * dt;
    float I = pid->Ki * pid->integral;
    float derivative = (error - pid->prev_error) / dt;
    float D = pid->Kd * derivative;

    float velocity = (setpoint - pid->prev_setpoint) / dt;
    float acceleration = (velocity - pid->prev_velocity) / dt;

    float FF = pid->ff0 + pid->ff1 * velocity + pid->ff2 * acceleration;

    float output = P + I + D + FF;

    if (output > pid->output_max){
        output = pid->output_max;
    } else if (output < -pid->output_max){
        output = -pid->output_max;
    }

    pid->prev_error = error;
    pid->prev_velocity = velocity;
    pid->prev_setpoint = setpoint;

    return output;
}

static int comp_id = -1;
static module_data_t *hal_data = 0;
static PID pid;

#include "hal_util.c"

static void process(void *arg, long period) {
    module_data_t *d = (module_data_t *)arg;
    static bool spindle_ref=false;
    static bool last_spindle_on=false;
    static int32_t previous_raw = 0;
    static int32_t raw_delta = 0;
    static uint8_t ref_state = 0;
    static uint32_t timeout = 0;
    static uint32_t error_timeout = 0;
    static uint32_t angle_timeout = 0;
    static bool firstrun = true;
    static bool error_trigger = false;
    static uint32_t ori_time = 0;
    static bool error_thrown = false;
    static bool old_spindle_index_enable;
    static bool old_spindle_x_index_enable;
    static bool last_key_switch = false;
    static uint32_t pid_cycles = 0;
    bool oriented = false;

    if (error_trigger){
        error_timeout ++;
        *d->error_trigg = 1;
        if (error_timeout > 1000){
            *d->error_trigg = 0;
            error_trigger = false;
            error_thrown = false;
            error_timeout = 0;
        }
    }

    if (firstrun)
        {
        PID_Init(&pid, *d->Kp, *d->Ki, *d->Kd, *d->out_min, *d->out_max, *d->ff0, *d->ff1, *d->ff2);
        firstrun = false;
    }

    if (*d->pid_reset && !*d->orient_in)
        {
        firstrun = true;
    }

    // error handling
    if (*d->orient_in || *d->spindle_on_in){
        if (!*d->chuck_ready){
            last_spindle_on = *d->spindle_on_in;
            error_trigger = true;
            if (!error_thrown){
                rtapi_print_msg(RTAPI_MSG_ERR, "Nincsen TOKMÁNY SZORíT!");
                error_thrown = true;
            }
            return;
        }
        if (!*d->tailstock_ready){
            last_spindle_on = *d->spindle_on_in;
            error_trigger = true;
            if (!error_thrown){
                rtapi_print_msg(RTAPI_MSG_ERR, "Szegnyereg hüvely hiba (nincs szorítás, vagy nincs hátul)!");
                error_thrown = true;
            }
            return;
        }
        if (!*d->key_switch){
            if (!*d->door_closed){
                last_spindle_on = *d->spindle_on_in;
                error_trigger = true;
                if (!error_thrown){
                    rtapi_print_msg(RTAPI_MSG_ERR, "Nyitott ajtóval nem lehet a főorsót forgatni!");
                    error_thrown = true;
                }
                return;
            }
        }
    }

    if (last_key_switch != *d->key_switch && *d->key_switch == 1){
        rtapi_print_msg(RTAPI_MSG_ERR, "Ajtó biztonság kapcsolóval kiiktatva, program indítás tiltva!");
    }

    last_key_switch = *d->key_switch;

    raw_delta = *d->spindle_raw_count - previous_raw;
    previous_raw = *d->spindle_raw_count;

    *d->spindle_angle = (*d->spindle_pos * 360000) / 1000;

    *d->angle_deviation = *d->spindle_angle - *d->orient_angle;

    // linuxcnc-be visszacsatoljuk a külső index-enable-t de csak változásnál mert nem tudunk orientálni különben
    if (!old_spindle_x_index_enable && *d->spindle_x_index_enable){
        *d->spindle_index_enable = 1;
    }
    if (old_spindle_x_index_enable && !*d->spindle_x_index_enable){
        *d->spindle_index_enable = 0;
    }

    if (!old_spindle_index_enable && *d->spindle_index_enable){
        *d->spindle_x_index_enable = 1;
    }
    if (old_spindle_index_enable && !*d->spindle_index_enable){
        *d->spindle_x_index_enable = 0;
    }

    old_spindle_x_index_enable = *d->spindle_x_index_enable;
    old_spindle_index_enable = *d->spindle_index_enable;

    if (!*d->spindle_not_locked && *d->orient_in == 1){
        *d->spindle_lock_out = 0;
        timeout = 250;
        return;
    }

    if (!*d->spindle_not_locked && *d->spindle_on_in == 1){
        *d->spindle_lock_out = 0;
        timeout = 250;
        return;
    }

    if (timeout > 0){
        timeout --;
        return;
    }

    oriented = fabsf(*d->angle_deviation) <= *d->locking_deviation;
    if (oriented){
        ori_time ++;
        if (ori_time > 1000){
            *d->is_oriented = 1;
            ori_time = 0;
            return;
        }
    } else {
        ori_time = 0;
    }

    if (*d->orient_in == 1 && ref_state == 0){
        if ((int32_t)*d->orient_angle % 3 != 0){
            if (!error_thrown){
                rtapi_print_msg(RTAPI_MSG_ERR, "M19 R hiba, a megadott szög nem osztható 3-al!");
                error_thrown = true;
            }
            *d->orient_in = 0;
            *d->orient_angle = 0;
            error_trigger = true;
            return;
        }
        *d->spindle_enable_out = 1;
        *d->spindle_slow_speed = 1;
        if (!spindle_ref){
            ref_state = 1;
            return;
            }
        *d->spindle_pid_command_out = *d->orient_angle / 360.0f;
        
        pid.Kp = *d->Kp;
        pid.Ki = *d->Ki;
        pid.Kd = *d->Kd;
        pid.ff0 = *d->ff0;
        pid.ff1 = *d->ff1;
        pid.ff2 = *d->ff2;

        if (pid_cycles % *d->pid_cycles == 0){
            *d->spindle_command_out = PID_Compute(&pid, *d->orient_angle / 360.0f, *d->spindle_pos, *d->pid_cycles * 0.001f);
        }
    } else if (*d->spindle_on_in){
        *d->spindle_command_out = *d->spindle_speed_in;
        *d->spindle_enable_out = 1;
        *d->spindle_slow_speed = 0;
        *d->is_oriented = 0;
        spindle_ref = false;

    } else if (*d->orient_in == 0 && *d->spindle_on_in == 0){
        *d->spindle_command_out = 0;
        *d->spindle_enable_out = 0;
        *d->spindle_slow_speed = 1;
        *d->is_oriented = 0;
        ori_time = 0;
        ref_state = 0;
    }
    switch (ref_state)
    {
    case 1:
        *d->spindle_command_out = 40;
        *d->spindle_index_enable = 1;
        ref_state = 2;
        break;
    case 2:
        if (*d->spindle_index_enable == 0){
            spindle_ref = true;
            ref_state = 0;
        }
        break;
    default:
        break;
    }

    last_spindle_on = *d->spindle_on_in;
    pid_cycles ++;
}

static int create_pins(void) {

    create_bit(&hal_data->orient_in, HAL_IN, "inputs.orient");
    create_float(&hal_data->locking_deviation, HAL_IN, "inputs.locking-deviation");
    create_bit(&hal_data->spindle_index_enable, HAL_IO, "index-enable");
    create_bit(&hal_data->spindle_x_index_enable, HAL_IO, "spindle-index-enable");
    create_float(&hal_data->orient_angle, HAL_IN, "inputs.orient-angle");
    create_bit(&hal_data->spindle_on_in, HAL_IN, "inputs.spindle-on");
    create_float(&hal_data->spindle_speed_in, HAL_IN, "inputs.spindle-speed");
    create_float(&hal_data->spindle_pid_command_out, HAL_OUT, "outputs.pid-command-out");
    create_float(&hal_data->spindle_pid_output_in, HAL_IN, "inputs.pid-output-in");
    create_float(&hal_data->spindle_pos, HAL_IN, "inputs.spindle-pos");
    create_s32(&hal_data->spindle_raw_count, HAL_IN, "inputs.spindle-raw-count");
    create_u32(&hal_data->pid_cycles, HAL_IN, "PID.cycles");
    create_float(&hal_data->spindle_command_out, HAL_OUT, "outputs.spindle-speed-cmd");
    // create_bit(&hal_data->spindle_index_out, HAL_IN, "outputs.index-out");
    create_bit(&hal_data->spindle_lock_out, HAL_IN, "outputs.locked-out");
    create_bit(&hal_data->is_oriented, HAL_IN, "outputs.is-oriented");
    create_bit(&hal_data->spindle_not_locked, HAL_IN, "inputs.spindle-not-locked");
    create_bit(&hal_data->spindle_enable_out, HAL_OUT, "outputs.spindle-on"); // analog enable
    create_float(&hal_data->spindle_angle, HAL_OUT, "debug.spindle-angle");
    create_float(&hal_data->angle_deviation, HAL_OUT, "debug.angle-deviation");
    create_float(&hal_data->Kp, HAL_IN, "PID.Kp");
    create_float(&hal_data->Ki, HAL_IN, "PID.Ki");
    create_float(&hal_data->Kd, HAL_IN, "PID.Kd");
    create_float(&hal_data->ff0, HAL_IN, "PID.FF0");
    create_float(&hal_data->ff1, HAL_IN, "PID.FF1");
    create_float(&hal_data->ff2, HAL_IN, "PID.FF2");
    create_float(&hal_data->out_min, HAL_IN, "PID.min");
    create_float(&hal_data->out_max, HAL_IN, "PID.max");
    create_bit(&hal_data->pid_reset, HAL_IN, "PID.reset");
    create_bit(&hal_data->chuck_ready, HAL_IN, "chuck.ready");
    create_bit(&hal_data->tailstock_ready, HAL_IN, "tailstock.ready");
    create_bit(&hal_data->door_closed, HAL_IN, "door-closed");
    create_bit(&hal_data->key_switch, HAL_IN, "key-switch");
    create_bit(&hal_data->error_trigg, HAL_OUT, "error-trigger");
    create_bit(&hal_data->spindle_slow_speed, HAL_OUT, "outputs.spindle-slow-speed");
    return 0;
}

int rtapi_app_main(void) {
    int r;
    char funct_name[64];

    rtapi_set_msg_level(RTAPI_MSG_INFO);

    comp_id = hal_init(module_name);
    if (comp_id < 0) {
        rtapi_print_msg(RTAPI_MSG_ERR, module_name ": hal_init failed (%d)\n", comp_id);
        return comp_id;
    }

    hal_data = hal_malloc(sizeof(module_data_t));
    if (!hal_data) {
        rtapi_print_msg(RTAPI_MSG_ERR, module_name ": hal_malloc failed\n");
        hal_exit(comp_id);
        return -ENOMEM;
    }

    memset(hal_data, 0, sizeof(module_data_t));

    r = create_pins();
    if (r < 0) {
        hal_exit(comp_id);
        return r;
    }

    memset(funct_name, 0, sizeof(funct_name));
    snprintf(funct_name, sizeof(funct_name), module_name ".process");
    r = hal_export_funct(funct_name, process, hal_data, 1, 0, comp_id);
    if (r < 0) {
        rtapi_print_msg(RTAPI_MSG_ERR, module_name ": hal_export_funct failed (%d)\n", r);
        hal_exit(comp_id);
        return r;
    }

    r = hal_ready(comp_id);
    if (r < 0) {
        rtapi_print_msg(RTAPI_MSG_ERR, module_name ": hal_ready failed (%d)\n", r);
        hal_exit(comp_id);
        return r;
    }

    rtapi_print_msg(RTAPI_MSG_INFO, module_name ": loaded with BLOCK_INPUT_COUNT=%d\n", BLOCK_INPUT_COUNT);
    return 0;
}

void rtapi_app_exit(void) {
    rtapi_print_msg(RTAPI_MSG_INFO, module_name ": unloading\n");
    if (comp_id >= 0) {
        hal_exit(comp_id);
    }
}
