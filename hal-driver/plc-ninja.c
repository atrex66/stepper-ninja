#include "rtapi.h"
#include "rtapi_app.h"
#include "rtapi_errno.h"
#include "hal.h"

#include <stdio.h>
#include <string.h>

#define module_name "plc-ninja"

#define MAX_IO_INDEX 32
#define MAX_OPS 64
#define MAX_LOGIC_LINE 512

MODULE_AUTHOR("Viola Zsolt");
MODULE_DESCRIPTION(module_name " HAL ladder interpreter");
MODULE_LICENSE("MIT");

typedef enum {
    OP_INPUT = 0,
    OP_INPUT_RISING,
    OP_INPUT_FALLING,
    OP_AND,
    OP_OR,
    OP_NOT,
    OP_TON,
    OP_TOF,
    OP_PULSE,
    OP_OUTPUT
} op_type_t;

typedef struct {
    op_type_t type;
    int index;
    int param;
} op_t;

typedef struct {
    unsigned long long elapsed_ns;
    unsigned char output;
    unsigned char pulse_active;
    unsigned char prev_stage_input;
} timer_state_t;

typedef struct {
    hal_bit_t *in[MAX_IO_INDEX];
    hal_bit_t *out[MAX_IO_INDEX];

    hal_u32_t *op_count_out;
    hal_bit_t *parse_ok_out;
    hal_bit_t *state_out;

    unsigned char inputs_used[MAX_IO_INDEX];
    unsigned char outputs_used[MAX_IO_INDEX];
    unsigned char prev_input[MAX_IO_INDEX];

    op_t ops[MAX_OPS];
    timer_state_t timers[MAX_OPS];
    int op_count;
} module_data_t;

static int comp_id = -1;
static module_data_t *hal_data = 0;
static char *logic_file = 0;

RTAPI_MP_STRING(logic_file, "Path to single-line PLC DSL file");

static int read_logic_file(char *buffer, size_t size);
static int parse_logic_line(const char *line, module_data_t *d);
static void process(void *arg, long period);
static int create_pins(void);
static int create_bit_pin(hal_bit_t **pin, int dir, const char *suffix);
static int create_u32_pin(hal_u32_t **pin, int dir, const char *suffix);
static int append_op(module_data_t *d, op_type_t type, int index, int param);
static void reset_timers(module_data_t *d);
static int parse_positive_int(const char **cursor, int *value);
static int is_blank_line(const char *line);

static int is_digit_char(char c) {
    return (c >= '0' && c <= '9');
}

static void skip_spaces(const char **cursor) {
    while (**cursor == ' ' || **cursor == '\t' || **cursor == '\r' || **cursor == '\n') {
        (*cursor)++;
    }
}

static int parse_index(const char **cursor, int *value) {
    int parsed = 0;
    int result = 0;

    while (is_digit_char(**cursor)) {
        result = (result * 10) + (**cursor - '0');
        (*cursor)++;
        parsed = 1;
    }

    if (!parsed) {
        return -EINVAL;
    }
    if (result < 0 || result >= MAX_IO_INDEX) {
        return -ERANGE;
    }

    *value = result;
    return 0;
}

static int parse_positive_int(const char **cursor, int *value) {
    int parsed = 0;
    int result = 0;

    while (is_digit_char(**cursor)) {
        result = (result * 10) + (**cursor - '0');
        (*cursor)++;
        parsed = 1;
    }

    if (!parsed) {
        return -EINVAL;
    }

    *value = result;
    return 0;
}

static int is_blank_line(const char *line) {
    while (*line == ' ' || *line == '\t' || *line == '\r' || *line == '\n') {
        line++;
    }

    return (*line == '\0');
}

static int append_op(module_data_t *d, op_type_t type, int index, int param) {
    if (d->op_count >= MAX_OPS) {
        rtapi_print_msg(RTAPI_MSG_ERR, module_name ": too many operations, limit is %d\n", MAX_OPS);
        return -E2BIG;
    }

    d->ops[d->op_count].type = type;
    d->ops[d->op_count].index = index;
    d->ops[d->op_count].param = param;
    d->op_count++;
    return 0;
}

static int parse_logic_line(const char *line, module_data_t *d) {
    const char *cursor = line;
    int index;
    int r;
    int saw_output = 0;

    memset(d->inputs_used, 0, sizeof(d->inputs_used));
    memset(d->outputs_used, 0, sizeof(d->outputs_used));
    memset(d->ops, 0, sizeof(d->ops));
    d->op_count = 0;

    skip_spaces(&cursor);
    if (*cursor == '\0') {
        rtapi_print_msg(RTAPI_MSG_ERR, module_name ": empty logic expression\n");
        return -EINVAL;
    }

    if (cursor[0] == 'I' && cursor[1] == 'R') {
        cursor += 2;
        r = parse_index(&cursor, &index);
        if (r < 0) return r;
        d->inputs_used[index] = 1;
        r = append_op(d, OP_INPUT_RISING, index, 0);
        if (r < 0) return r;
    } else if (cursor[0] == 'I' && cursor[1] == 'F') {
        cursor += 2;
        r = parse_index(&cursor, &index);
        if (r < 0) return r;
        d->inputs_used[index] = 1;
        r = append_op(d, OP_INPUT_FALLING, index, 0);
        if (r < 0) return r;
    } else if (cursor[0] == 'I') {
        cursor += 1;
        r = parse_index(&cursor, &index);
        if (r < 0) return r;
        d->inputs_used[index] = 1;
        r = append_op(d, OP_INPUT, index, 0);
        if (r < 0) return r;
    } else {
        rtapi_print_msg(RTAPI_MSG_ERR, module_name ": expression must start with I<n>, IR<n>, or IF<n>\n");
        return -EINVAL;
    }

    while (1) {
        skip_spaces(&cursor);
        if (*cursor == '\0') {
            break;
        }

        if (*cursor == '&') {
            cursor++;
            r = parse_index(&cursor, &index);
            if (r < 0) return r;
            d->inputs_used[index] = 1;
            r = append_op(d, OP_AND, index, 0);
            if (r < 0) return r;
            continue;
        }

        if (*cursor == '|') {
            cursor++;
            r = parse_index(&cursor, &index);
            if (r < 0) return r;
            d->inputs_used[index] = 1;
            r = append_op(d, OP_OR, index, 0);
            if (r < 0) return r;
            continue;
        }

        if (*cursor == '!') {
            cursor++;
            r = append_op(d, OP_NOT, -1, 0);
            if (r < 0) return r;
            continue;
        }

        if (*cursor == 'T' || *cursor == 'F' || *cursor == 'P') {
            op_type_t type = OP_TON;
            int delay_ms;
            if (*cursor == 'F') {
                type = OP_TOF;
            } else if (*cursor == 'P') {
                type = OP_PULSE;
            }

            cursor++;
            r = parse_positive_int(&cursor, &delay_ms);
            if (r < 0) return r;
            r = append_op(d, type, -1, delay_ms);
            if (r < 0) return r;
            continue;
        }

        if (*cursor == 'O') {
            cursor++;
            r = parse_index(&cursor, &index);
            if (r < 0) return r;
            d->outputs_used[index] = 1;
            r = append_op(d, OP_OUTPUT, index, 0);
            if (r < 0) return r;
            saw_output = 1;
            skip_spaces(&cursor);
            if (*cursor != '\0') {
                rtapi_print_msg(RTAPI_MSG_ERR, module_name ": output token must be the last token\n");
                return -EINVAL;
            }
            break;
        }

        rtapi_print_msg(RTAPI_MSG_ERR, module_name ": unsupported token near '%s'\n", cursor);
        return -EINVAL;
    }

    if (!saw_output) {
        rtapi_print_msg(RTAPI_MSG_ERR, module_name ": expression must terminate with O<n>\n");
        return -EINVAL;
    }

    if (d->op_count < 2) {
        rtapi_print_msg(RTAPI_MSG_ERR, module_name ": expression is incomplete\n");
        return -EINVAL;
    }

    return 0;
}

static int read_logic_file(char *buffer, size_t size) {
    FILE *fp;
    int found_line = 0;

    if (!logic_file || logic_file[0] == '\0') {
        rtapi_print_msg(RTAPI_MSG_ERR, module_name ": logic_file module parameter is required\n");
        return -EINVAL;
    }

    fp = fopen(logic_file, "r");
    if (!fp) {
        rtapi_print_msg(RTAPI_MSG_ERR, module_name ": failed to open logic file '%s'\n", logic_file);
        return -ENOENT;
    }

    buffer[0] = '\0';
    while (fgets(buffer, (int)size, fp)) {
        buffer[size - 1] = '\0';
        if (!is_blank_line(buffer)) {
            found_line = 1;
            break;
        }
    }

    if (!found_line) {
        if (ferror(fp)) {
            fclose(fp);
            rtapi_print_msg(RTAPI_MSG_ERR, module_name ": failed to read logic file '%s'\n", logic_file);
            return -EIO;
        }

        fclose(fp);
        rtapi_print_msg(RTAPI_MSG_ERR, module_name ": logic file '%s' is empty\n", logic_file);
        return -EINVAL;
    }

    fclose(fp);
    buffer[size - 1] = '\0';
    return 0;
}

static int create_bit_pin(hal_bit_t **pin, int dir, const char *suffix) {
    int r;
    char name[96];

    memset(name, 0, sizeof(name));
    snprintf(name, sizeof(name), module_name ".%s", suffix);
    r = hal_pin_bit_newf(dir, pin, comp_id, "%s", name);
    if (r < 0) {
        rtapi_print_msg(RTAPI_MSG_ERR, module_name ": failed to create pin %s (err=%d)\n", name, r);
        return r;
    }
    return 0;
}

static int create_u32_pin(hal_u32_t **pin, int dir, const char *suffix) {
    int r;
    char name[96];

    memset(name, 0, sizeof(name));
    snprintf(name, sizeof(name), module_name ".%s", suffix);
    r = hal_pin_u32_newf(dir, pin, comp_id, "%s", name);
    if (r < 0) {
        rtapi_print_msg(RTAPI_MSG_ERR, module_name ": failed to create pin %s (err=%d)\n", name, r);
        return r;
    }
    return 0;
}

static int create_pins(void) {
    int i;
    int r;
    char name[32];

    for (i = 0; i < MAX_IO_INDEX; i++) {
        if (hal_data->inputs_used[i]) {
            memset(name, 0, sizeof(name));
            snprintf(name, sizeof(name), "I%d", i);
            r = create_bit_pin(&hal_data->in[i], HAL_IN, name);
            if (r < 0) return r;
        }

        if (hal_data->outputs_used[i]) {
            memset(name, 0, sizeof(name));
            snprintf(name, sizeof(name), "O%d", i);
            r = create_bit_pin(&hal_data->out[i], HAL_OUT, name);
            if (r < 0) return r;
            *hal_data->out[i] = 0;
        }
    }

    r = create_u32_pin(&hal_data->op_count_out, HAL_OUT, "op-count");
    if (r < 0) return r;

    r = create_bit_pin(&hal_data->parse_ok_out, HAL_OUT, "parse-ok");
    if (r < 0) return r;

    r = create_bit_pin(&hal_data->state_out, HAL_OUT, "state");
    if (r < 0) return r;

    *hal_data->op_count_out = (hal_u32_t)hal_data->op_count;
    *hal_data->parse_ok_out = 1;
    *hal_data->state_out = 0;
    return 0;
}

static hal_bit_t update_ton(timer_state_t *timer, hal_bit_t input, unsigned long long period_ns, int delay_ms) {
    unsigned long long delay_ns = ((unsigned long long)delay_ms) * 1000000ULL;

    if (!input) {
        timer->elapsed_ns = 0;
        timer->output = 0;
        return 0;
    }

    if (timer->elapsed_ns < delay_ns) {
        timer->elapsed_ns += period_ns;
        if (timer->elapsed_ns > delay_ns) {
            timer->elapsed_ns = delay_ns;
        }
    }

    timer->output = (timer->elapsed_ns >= delay_ns) ? 1 : 0;
    return timer->output;
}

static hal_bit_t update_tof(timer_state_t *timer, hal_bit_t input, unsigned long long period_ns, int delay_ms) {
    unsigned long long delay_ns = ((unsigned long long)delay_ms) * 1000000ULL;

    if (input) {
        timer->elapsed_ns = 0;
        timer->output = 1;
        return 1;
    }

    if (timer->output) {
        timer->elapsed_ns += period_ns;
        if (timer->elapsed_ns >= delay_ns) {
            timer->elapsed_ns = delay_ns;
            timer->output = 0;
        }
    }

    return timer->output;
}

static hal_bit_t update_pulse(timer_state_t *timer, hal_bit_t input, hal_bit_t prev_input, unsigned long long period_ns, int delay_ms) {
    unsigned long long delay_ns = ((unsigned long long)delay_ms) * 1000000ULL;
    hal_bit_t rising = (!prev_input && input) ? 1 : 0;

    if (rising && !timer->pulse_active) {
        timer->pulse_active = 1;
        timer->elapsed_ns = 0;
        timer->output = 1;
    }

    if (timer->pulse_active) {
        timer->elapsed_ns += period_ns;
        if (timer->elapsed_ns >= delay_ns) {
            timer->pulse_active = 0;
            timer->elapsed_ns = delay_ns;
            timer->output = 0;
        }
    }

    timer->prev_stage_input = input;

    return timer->output;
}

static void reset_timers(module_data_t *d) {
    memset(d->timers, 0, sizeof(d->timers));
}

static void process(void *arg, long period) {
    module_data_t *d = (module_data_t *)arg;
    hal_bit_t state = 0;
    unsigned char curr_input[MAX_IO_INDEX];
    int i;
    unsigned long long period_ns = (period > 0) ? (unsigned long long)period : 0ULL;

    for (i = 0; i < MAX_IO_INDEX; i++) {
        curr_input[i] = (d->inputs_used[i] && d->in[i] && *d->in[i]) ? 1 : 0;
    }

    for (i = 0; i < d->op_count; i++) {
        op_t *op = &d->ops[i];
        switch (op->type) {
            case OP_INPUT:
                state = curr_input[op->index];
                break;

            case OP_INPUT_RISING:
                state = (!d->prev_input[op->index] && curr_input[op->index]) ? 1 : 0;
                break;

            case OP_INPUT_FALLING:
                state = (d->prev_input[op->index] && !curr_input[op->index]) ? 1 : 0;
                break;

            case OP_AND:
                state = (state && curr_input[op->index]) ? 1 : 0;
                break;

            case OP_OR:
                state = (state || curr_input[op->index]) ? 1 : 0;
                break;

            case OP_NOT:
                state = state ? 0 : 1;
                break;

            case OP_TON:
                state = update_ton(&d->timers[i], state, period_ns, op->param);
                break;

            case OP_TOF:
                state = update_tof(&d->timers[i], state, period_ns, op->param);
                break;

            case OP_PULSE:
                state = update_pulse(&d->timers[i], state, d->timers[i].prev_stage_input, period_ns, op->param);
                break;

            case OP_OUTPUT:
                if (d->out[op->index]) {
                    *d->out[op->index] = state;
                }
                break;

            default:
                break;
        }
    }

    for (i = 0; i < MAX_IO_INDEX; i++) {
        if (d->inputs_used[i]) {
            d->prev_input[i] = curr_input[i];
        }
    }

    *d->state_out = state;
}

int rtapi_app_main(void) {
    int r;
    char funct_name[64];
    char logic_line[MAX_LOGIC_LINE];

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

    r = read_logic_file(logic_line, sizeof(logic_line));
    if (r < 0) {
        hal_exit(comp_id);
        return r;
    }

    r = parse_logic_line(logic_line, hal_data);
    if (r < 0) {
        hal_exit(comp_id);
        return r;
    }

    r = create_pins();
    if (r < 0) {
        hal_exit(comp_id);
        return r;
    }

    reset_timers(hal_data);

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

    rtapi_print_msg(RTAPI_MSG_INFO, module_name ": loaded logic '%s' with %d ops\n", logic_file, hal_data->op_count);
    return 0;
}

void rtapi_app_exit(void) {
    rtapi_print_msg(RTAPI_MSG_INFO, module_name ": unloading\n");
    if (comp_id >= 0) {
        hal_exit(comp_id);
    }
}