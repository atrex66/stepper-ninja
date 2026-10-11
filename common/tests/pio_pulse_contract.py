#!/usr/bin/env python3
"""Execute the actual pulse PIO instruction text; check count and cycle timing."""
import pathlib
root = pathlib.Path(__file__).resolve().parents[2]
lines = (root / "firmware/pio/mach3_step.pio").read_text().splitlines()
code, labels = [], {}
for line in lines:
    line = line.split(';')[0].strip()
    if not line or line.startswith('.'):
        continue
    if line.endswith(':'):
        labels[line[:-1]] = len(code)
    else:
        code.append(line.replace(',', '').split())

def run(count, high, low):
    reg = dict(x=0, y=0, osr=0, isr=0)
    fifo = [count - 1, high, low]
    pc, cycle, pin = 0, 0, 0
    rises, falls = [], []
    while cycle < 100000:
        op, *args = code[pc]
        pc += 1
        if op == 'pull':
            assert fifo
            reg['osr'] = fifo.pop(0)
        elif op == 'mov':
            reg[args[0]] = reg[args[1]]
        elif op == 'set':
            assert args[0] == 'pins'
            pin = int(args[1])
            (rises if pin else falls).append(cycle)
        elif op == 'jmp':
            key = args[0][:-2]
            before = reg[key]
            reg[key] = (before - 1) & 0xffffffff
            if before:
                pc = labels[args[1]]
        elif op == 'irq':
            assert pin == 0 and len(rises) == len(falls) == count
            assert all(f - r == high + 2 for r, f in zip(rises, falls))
            assert all(b - a == high + low + 7 for a, b in zip(rises, rises[1:]))
            return cycle + 1
        else:
            raise AssertionError(op)
        cycle += 1
    raise AssertionError('PIO did not finish')
for count in (1, 2, 10, 80):
    for high in (8, 23, 98):
        for low in (8, 32, 100):
            cycles = run(count, high, low)
            assert cycles == 5 + count * (high + low + 7) + 1
# Step-counter transition table: DIR=0 counts rising edges; DIR=1 falling edges.
# Check the actual computed-jump entries instead of assuming a direction sign.
counter = (root / 'firmware/pio/step_counter.pio').read_text()
assert 'JMP increment ; read 01' in counter
assert 'JMP decrement ; read 11' in counter
print('PASS: real PIO text, exact pulses, high/period timing and counter sign')
