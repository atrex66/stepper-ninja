# plc-ninja

Author: Viola Zsolt  
License: MIT  
HAL component name: `plc-ninja`

## Overview

`plc-ninja` is a small LinuxCNC realtime HAL component that evaluates a
single-line PLC-style logic expression.

The component is designed as a deterministic ladder-like interpreter:

- the logic file is parsed once at module load time
- the expression is converted into a fixed linear pipeline of operations
- only the HAL pins actually used by the expression are exported
- the realtime loop runs without dynamic allocation or reparsing

This makes it suitable for simple interlocks, edge-triggered actions,
delayed enables, output pulses, and small machine-side PLC tasks.

## Design Limits

The current implementation has these fixed limits:

- maximum inputs: `32`
- maximum outputs: `32`
- maximum operations in one expression: `64`
- maximum logic line length: `512` characters
- one logic expression per module instance

## Runtime Model

The interpreter evaluates logic strictly from left to right.

There is:

- no branching
- no expression tree
- no operator precedence
- no recursion

This means the DSL behaves like a pipeline, not like a general-purpose boolean parser.

Example:

```text
I0 &1 ! T100 O0
```

is evaluated as:

```text
state = I0
state = state && I1
state = !state
state = TON(100 ms)
O0 = state
```

## Module Parameter

The component requires a module parameter named `logic_file`.

Example:

```hal
loadrt plc-ninja logic_file=/home/ninja/io-ninja/hal-driver/test_config/plc.logic
```

The file must contain a single-line expression. The component reads the first line during `rtapi_app_main()`.

## DSL Syntax

### Inputs

- `I0` means input `0`
- `I1` means input `1`
- `I31` means input `31`

### Edge Inputs

- `IR0` means rising edge detect on input `0`
- `IF0` means falling edge detect on input `0`

These produce a one-cycle boolean result based on the previous sampled state of the input.

### Boolean Operators

- `&1` means logical AND with input `1`
- `|2` means logical OR with input `2`
- `!` means logical NOT of the current pipeline state

### Timing Operators

- `T100` means TON on-delay with `100 ms`
- `F100` means TOF off-delay with `100 ms`
- `P100` means output pulse for `100 ms`

The number after `T`, `F`, or `P` is interpreted as milliseconds.

### Output

- `O0` means assign the final pipeline state to output `0`

The output token must be the last token in the line.

## Valid Expression Rules

An expression must follow these rules:

- it must begin with `I<n>`, `IR<n>`, or `IF<n>`
- it is processed strictly left to right
- it must end with `O<n>`
- nothing may appear after the output token
- spaces are optional separators and extra spaces are ignored

Valid examples:

```text
I0 O0
I0 &1 O0
IR0 P200 O1
I0 ! O0
I0 &1 |2 O3
I0 T500 O0
I0 F250 O2
```

Invalid examples:

```text
O0 I0
&1 I0 O0
I0 O0 &1
I0 T O0
I0 I1 O0
```

## HAL Pins

All pins are prefixed with `plc-ninja.`

### Auto-Created Input Pins

Only inputs referenced by the expression are exported.

Examples:

- `plc-ninja.I0`
- `plc-ninja.I1`
- `plc-ninja.I7`

Type: `bit`, Direction: `IN`

### Auto-Created Output Pins

Only outputs referenced by the expression are exported.

Examples:

- `plc-ninja.O0`
- `plc-ninja.O1`

Type: `bit`, Direction: `OUT`

### Status Pins

These are always exported:

| Pin | Type | Direction | Meaning |
|---|---|---|---|
| `plc-ninja.op-count` | `u32` | OUT | Number of parsed pipeline operations |
| `plc-ninja.parse-ok` | `bit` | OUT | Set to `1` when the expression parsed successfully |
| `plc-ninja.state` | `bit` | OUT | Final pipeline state from the current cycle |

### Realtime Function

The exported realtime function is:

```text
plc-ninja.process
```

This should be added to the servo thread.

## Timer Behavior

### TON: `T<ms>`

TON delays a false-to-true transition.

Behavior:

- if input is `0`, timer resets and output is `0`
- if input is `1`, elapsed time accumulates
- once elapsed time reaches the configured delay, output becomes `1`

### TOF: `F<ms>`

TOF delays a true-to-false transition.

Behavior:

- if input is `1`, output is immediately `1`
- when input goes `0`, the timer starts counting down the off-delay
- output remains `1` until the configured delay expires

### PULSE: `P<ms>`

Pulse generates a fixed-length `1` pulse on the rising edge of the current pipeline state.

Behavior:

- a rising edge starts the pulse
- output stays `1` for the configured time
- output then returns to `0`
- additional high level time does not retrigger until a new rising edge occurs

## Internal Execution Model

The component compiles the expression into an internal op list like this:

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

Each cycle:

1. used input pins are sampled
2. the op list is executed in order
3. the selected output pin is written
4. previous input states are updated for edge detection

This keeps execution cost fixed and predictable for a given expression.

## Example Logic Files

### Basic interlock

```text
I0 &1 O0
```

Meaning:

- output `O0` is on only if `I0` and `I1` are both true

### Inverted permit with delay

```text
I0 ! T500 O0
```

Meaning:

- start from `I0`
- invert it
- apply 500 ms on-delay
- write the result to `O0`

### Rising-edge pulse

```text
IR0 P150 O1
```

Meaning:

- detect a rising edge on `I0`
- generate a 150 ms pulse
- write it to `O1`

### Delayed dropout

```text
I0 F1000 O2
```

Meaning:

- `O2` turns on immediately with `I0`
- `O2` stays on for 1 second after `I0` goes false

## LinuxCNC HAL Example

Example logic file:

```text
I0 &1 T250 O0
```

Example HAL wiring:

```hal
loadrt plc-ninja logic_file=/home/ninja/io-ninja/hal-driver/test_config/plc.logic
addf plc-ninja.process servo-thread

# machine enable and guard condition
net machine-enabled    some-signal-a  => plc-ninja.I0
net guard-closed       some-signal-b  => plc-ninja.I1

# delayed output result
net plc-output-enable  plc-ninja.O0   => some-output-signal
```

## Error Conditions

The component refuses to load if:

- `logic_file` is missing
- the file cannot be opened
- the first line cannot be read
- the expression does not start with an input token
- the expression does not end with an output token
- an unsupported token is encountered
- an input or output index is outside `0..31`
- the operation count exceeds `64`

Parser errors are reported through `rtapi_print_msg()` during module load.

## Notes

- the component currently supports one expression and one realtime function per module instance
- only the first line of the file is read
- there is no support for parentheses or precedence
- timer values are in milliseconds, but runtime accumulation uses the realtime thread period in nanoseconds
- `plc-ninja.state` mirrors the final boolean result of the current pipeline cycle

## Build

The module is registered in the HAL driver CMake build and can be built with:

```bash
cmake -S /home/ninja/io-ninja/hal-driver -B /home/ninja/io-ninja/hal-driver/build-cmake
cmake --build /home/ninja/io-ninja/hal-driver/build-cmake --target plc-ninja
```