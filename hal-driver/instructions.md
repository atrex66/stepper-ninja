# Mini HAL Ladder Interpreter – Instructions for VSCode Agent

## 🎯 Goal
Create a LinuxCNC HAL component (C-based) that:

- Reads a **single-line logic expression** from a file
- Parses it into a **pipeline of operations**
- Automatically creates HAL pins based on used inputs/outputs
- Evaluates the logic in realtime (deterministic, no dynamic allocation)

---

## 🧩 Core Design Constraints

- Single-line logic only
- No branching / no tree structure
- Strict **left-to-right pipeline evaluation**
- No operator precedence
- No recursion
- No dynamic memory in realtime loop
- Parsing happens only at init

---

## 🔤 DSL Syntax

### Inputs

- `I0` → input 0
- `IR0` → rising edge of input 0
- `IF0` → falling edge of input 0

### Outputs

- `O0` → output 0 (must appear at end)

### Operators (pipeline stages)

- `&1` → AND with input 1
- `|2` → OR with input 2
- `!` → NOT

### Timing

- `T100` → TON (on-delay) 100 ms
- `F100` → TOF (off-delay) 100 ms
- `P100` → pulse 100 ms

### Example

```
I0 &1 ! T100 O0
```

Meaning:
```
state = I0
state = state && I1
state = !state
state = TON(100ms)
O0 = state
```

---

## 🧠 Internal Representation

Define an operation enum:

```c
typedef enum {
    OP_INPUT,
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
```

Operation struct:

```c
typedef struct {
    op_type_t type;
    int index;     // input/output index
    int param;     // time in ms
} op_t;
```

---

## 📥 Parsing Phase (Init Only)

### Steps

1. Read file into string
2. Iterate char-by-char
3. Identify tokens
4. Build `op_t ops[]`
5. Track used inputs/outputs

### Tracking usage

```c
bool inputs_used[32] = {0};
bool outputs_used[32] = {0};
```

---

## 🔌 HAL Pin Creation

After parsing:

```c
for (int i = 0; i < 32; i++) {
    if (inputs_used[i]) {
        hal_pin_bit_newf(HAL_IN, &in[i], comp_id, "I%d", i);
    }
    if (outputs_used[i]) {
        hal_pin_bit_newf(HAL_OUT, &out[i], comp_id, "O%d", i);
    }
}
```

---

## ⚙️ Runtime Execution

### State variables

```c
bool state;
bool prev[32];
```

### Main loop

```c
state = read_input(ops[0]);

for (int i = 1; i < op_count; i++) {
    switch (ops[i].type) {

        case OP_AND:
            state &= in[ops[i].index];
            break;

        case OP_OR:
            state |= in[ops[i].index];
            break;

        case OP_NOT:
            state = !state;
            break;

        case OP_INPUT_RISING: {
            bool curr = in[ops[i].index];
            state = (!prev[ops[i].index] && curr);
            prev[ops[i].index] = curr;
            break;
        }

        case OP_INPUT_FALLING: {
            bool curr = in[ops[i].index];
            state = (prev[ops[i].index] && !curr);
            prev[ops[i].index] = curr;
            break;
        }

        case OP_TON:
            state = ton_update(&timers[i], state);
            break;
    }
}

out[output_index] = state;
```

---

## ⏱️ Timer Implementation (TON)

```c
typedef struct {
    int delay_ms;
    int counter;
    bool output;
} ton_t;
```

Behavior:

- If input true → increment counter
- If counter >= delay → output true
- If input false → reset

---

## 📁 File Format

Single line:

```
I0 &1 T100 O0
```

No newlines required.

---

## 🚫 Constraints for Agent

- Do NOT use malloc in realtime code
- Do NOT use recursion
- Do NOT re-parse at runtime
- Fixed-size arrays only
- Deterministic execution time

---

## ✅ Deliverables

Agent should generate:

1. `hal_ladder.comp` (LinuxCNC component wrapper)
2. `hal_ladder.c` (core logic)
3. Parser function
4. Runtime execution loop
5. Timer implementation

---

## 💡 Optional Extensions

- XOR operator (`^`)
- Toggle (`G`)
- Delay line (`D`)

---

## 🏁 Summary

This is a **linear logic pipeline interpreter**:

- Compile once
- Execute fast
- Fully deterministic
- Minimal overhead

Target: ~100–200 lines of clean C code.

