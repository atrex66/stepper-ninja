#include "rtapi.h"              /* RTAPI realtime OS API */
#include "rtapi_app.h"          /* RTAPI realtime module decls */
#include "rtapi_errno.h"        /* EINVAL etc */
#include "hal.h"                /* HAL public API decls */

#include <string.h>
#include <stdio.h>

#define debug 1
#define module_name "chuck-handler"

/* module information */
MODULE_AUTHOR("Viola Zsolt");
MODULE_DESCRIPTION(module_name " driver");
MODULE_LICENSE("MIT");

typedef struct {

    // machine specific data for HAL start
    hal_bit_t *chuck_inhibit_in;    // chuck inhibit input pin (to prevent the chuck from moving when the machine is in a state that should not allow chuck movement)
    hal_bit_t *chuck_switch_in;     // toggle switch input pin (for single foot pedal)
    hal_bit_t *chuck_close_in;      // chuck close input pin (for double foot pedal)
    hal_bit_t *chuck_open_in;       // chuck open input pin (for double foot pedal)
    
    hal_bit_t *chuck_od_limit_in;   // chuck outer diameter limit input pin (to prevent the chuck from moving when the outer diameter of the workpiece exceeds a certain limit)
    hal_bit_t *chuck_id_limit_in;   // chuck inner diameter limit input pin (to prevent the chuck from moving when the inner diameter of the workpiece is below a certain limit)

    hal_bit_t *chuck_pressure_od_in;// pressure sensor input pin for outer diameter (to prevent the chuck from moving when the pressure exceeds a certain limit)
    hal_bit_t *chuck_pressure_id_in;// pressure sensor input pin for inner diameter (to prevent the chuck from moving when the pressure exceeds a certain limit)
    
    hal_bit_t *chuck_direction_od;  // outer direction pin (to control the direction of the chuck for outer diameter)
    hal_bit_t *chuck_direction_id;  // inner direction pin (to control the direction of the chuck for inner diameter)

    hal_bit_t *chuck_close_out;       // chuck close pin (connect to the chuck close output pin of the machine) (reversed logic when IO or OD state is active)
    hal_bit_t *chuck_open_out;        // chuck open pin (connect to the chuck open output pin of the machine)
    hal_bit_t *chuck_ready_out;       // chuck ready pin (to indicate that the chuck is ready for operation)
    hal_bit_t *chuck_fault_out;       // chuck fault pin (to indicate that a fault state is detected in the chuck, e.g. both directions are active or both directions are inactive while one of the switch inputs is active)
    hal_bit_t *fault_trigger_out;     // fault trigger pin (to trigger a fault state in the machine when a fault state is detected in the chuck, e.g. both directions are active or both directions are inactive while one of the switch inputs is active)
    hal_bit_t *io_ready_in;           // need to connect in the machine emergency stop circuit to prevent the chuck opening when spindle is running
    hal_bit_t *io_ready_out;          // need to connect in the machine emergency stop circuit to prevent the spindle from running when the chuck is opening
    // machine specific data for HAL end

} module_data_t;

static int comp_id = -1; // HAL komponens azonosító
static module_data_t *hal_data; // Pointer a megosztott memóriában lévő adatra

static uint8_t toggle_state = 0;      // 0 = open/released, 1 = closed/gripping
static bool last_toggle_input = false; // previous state of toggle switch for edge detection
static bool first_toggle_seen = false; // force first toggle command to clamp
#define _close 1
#define _open 2

#include "hal_util.c"

void process(void *arg, long period) {
    module_data_t *d = arg;
    bool switch_now;

    bool od         = *d->chuck_direction_od;
    bool id         = *d->chuck_direction_id;
    bool any_switch = *d->chuck_switch_in || *d->chuck_close_in || *d->chuck_open_in;
    bool id_pressure_ok  = !*d->chuck_id_limit_in && *d->chuck_pressure_id_in && id; // selected ID direction has no active limit and has pressure
    bool od_pressure_ok  = !*d->chuck_od_limit_in && *d->chuck_pressure_od_in && od; // selected OD direction has no active limit and has pressure
    bool no_limit_switch = !*d->chuck_id_limit_in && !*d->chuck_od_limit_in;
    bool pressure_by_dir = (id && *d->chuck_pressure_id_in) || (od && *d->chuck_pressure_od_in);

    // --- Direction fault: both active or both inactive while a switch is pressed ---
    if ((od && id) || (!od && !id)) {
        if (any_switch && !*d->fault_trigger_out) {
            rtapi_print_msg(RTAPI_MSG_ERR, module_name ": Fault state detected: "
                "both direction pins active or both inactive while a movement input is active. "
                "Inhibiting chuck movement.\n");
        }
        *d->chuck_close_out   = 0;
        *d->chuck_open_out    = 0;
        *d->chuck_ready_out   = 0;
        *d->chuck_fault_out   = 1;
        *d->fault_trigger_out = any_switch ? 1 : 0;
        *d->io_ready_out      = 0;
        return;
    }

    if (any_switch){
        if (toggle_state == 0 && od) {
            toggle_state = _close; // force close if outer direction is active
        } else if (toggle_state == 0 && id) {
            toggle_state = _open; // force open if inner direction is active
        }
    }

    switch_now = *d->chuck_switch_in ? true : false;
    if (switch_now && !last_toggle_input) {
        if (toggle_state == _close) {
            toggle_state = _open;
        } else {
            toggle_state = _close;
        }
    }
    last_toggle_input = switch_now;

    *d->chuck_close_out   = (toggle_state == _close) ? 1 : 0; // only close if inner diameter direction is active and pressure/limit conditions are ok
    *d->chuck_open_out    = (toggle_state == _open) ? 1 : 0; // only open if outer diameter direction is active and pressure/limit conditions are ok
    *d->chuck_ready_out   = (no_limit_switch && pressure_by_dir && !*d->fault_trigger_out) ? 1 : 0;
    *d->io_ready_out      = (*d->chuck_close_out || *d->chuck_open_out) ? 0 : 1; // if chuck is moving, IO is not ready to allow spindle to run

}


void creation_of_hal_pins(void) {
    // pin for inhibit of the chuck movement
    create_bit(&hal_data->chuck_inhibit_in, HAL_IN, "inputs.inhibit");
    // pin for dasy chain the ready state
    create_bit(&hal_data->io_ready_in, HAL_IN, "inputs.io-ready");
    // pin for dasy chain the ready state
    create_bit(&hal_data->io_ready_out, HAL_OUT, "outputs.io-ready");

    // pin for toggle between clamp and open, can be connected to a single foot pedal
    create_bit(&hal_data->chuck_switch_in, HAL_IN, "inputs.open-close");

    // pins for clamp and open, can be connected to a double foot pedal
    create_bit(&hal_data->chuck_close_in, HAL_IN, "inputs.close");
    create_bit(&hal_data->chuck_open_in, HAL_IN, "inputs.open");

    // pins for outer diameter and inner diameter limit inputs
    create_bit(&hal_data->chuck_od_limit_in, HAL_IN, "inputs.od-limit");
    create_bit(&hal_data->chuck_id_limit_in, HAL_IN, "inputs.id-limit");

    // pins for pressure sensor inputs for outer diameter and inner diameter
    create_bit(&hal_data->chuck_pressure_od_in, HAL_IN, "inputs.pressure-od");
    create_bit(&hal_data->chuck_pressure_id_in, HAL_IN, "inputs.pressure-id");

    // pins for direction control for outer diameter and inner diameter
    create_bit(&hal_data->chuck_direction_od, HAL_IN, "inputs.direction-od");
    create_bit(&hal_data->chuck_direction_id, HAL_IN, "inputs.direction-id");

    // pins for clamp and open outputs (solenoid valves)
    create_bit(&hal_data->chuck_close_out, HAL_OUT, "outputs.close");
    create_bit(&hal_data->chuck_open_out, HAL_OUT, "outputs.open");

    // status output pins
    create_bit(&hal_data->chuck_ready_out, HAL_OUT, "outputs.ready");
    create_bit(&hal_data->chuck_fault_out, HAL_OUT, "outputs.fault");

    // fault trigger output (latches on movement attempted with invalid direction)
    create_bit(&hal_data->fault_trigger_out, HAL_OUT, "outputs.fault-trigger");
}

int rtapi_app_main(void) {
    int r;

    rtapi_set_msg_level(RTAPI_MSG_INFO);

    // Allocate memory for hal_data in shared memory
    hal_data = hal_malloc(sizeof(module_data_t));
    if (hal_data == NULL) {
        rtapi_print_msg(RTAPI_MSG_ERR, module_name ": hal_data allocation failed\n");
        return -1;
    }

    // Initialize hal component
    comp_id = hal_init(module_name);
    if (comp_id < 0) {
        rtapi_print_msg(RTAPI_MSG_ERR, module_name ": hal_init failed: %d\n", comp_id);
        return comp_id;
    }

    char name[64] = {0};
    // creating user HAL pins

        creation_of_hal_pins();
        
    // end of creating user HAL pins

    #pragma message "Adding export functions. (process-recv)"
    create_process("process", process, hal_data);
    rtapi_print_msg(RTAPI_MSG_INFO, module_name ": hal_export_funct for process:\n");

    r = hal_ready(comp_id);
    if (r < 0) {
        rtapi_print_msg(RTAPI_MSG_ERR, module_name ": hal_ready failed\n");
        hal_exit(comp_id);
        return r;
    }

    rtapi_print_msg(RTAPI_MSG_INFO, module_name ": component ready.\n");
    return 0;
}

void rtapi_app_exit(void) {
    rtapi_print_msg(RTAPI_MSG_INFO, module_name ": Exiting component\n");
    hal_exit(comp_id);
}
