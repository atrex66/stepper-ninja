from gui import *

pico1 = load_component("components/pico.json")

pico = Node(600, 0, 100, 100, "pico.png", 5, "Pico", 20, 20)
pico.pin_y_offs = 3

pico.add_pin(Pin(1, "GPIO0", Shape.RECTANGLE, Side.LEFT, PinType.NOT_USED))
pico.add_pin(Pin(2, "GPIO1", Shape.RECTANGLE, Side.LEFT, PinType.NOT_USED))
pico.add_pin(Pin(3, "GND", Shape.RECTANGLE, Side.LEFT, PinType.UNUSABLE))
pico.add_pin(Pin(4, "GPIO2", Shape.RECTANGLE, Side.LEFT, PinType.NOT_USED))
pico.add_pin(Pin(5, "GPIO3", Shape.RECTANGLE, Side.LEFT, PinType.NOT_USED))
pico.add_pin(Pin(6, "GPIO4", Shape.RECTANGLE, Side.LEFT, PinType.NOT_USED))
pico.add_pin(Pin(7, "GPIO5", Shape.RECTANGLE, Side.LEFT, PinType.NOT_USED))
pico.add_pin(Pin(8, "GND", Shape.RECTANGLE, Side.LEFT, PinType.UNUSABLE))
pico.add_pin(Pin(9, "GPIO6", Shape.RECTANGLE, Side.LEFT, PinType.NOT_USED))
pico.add_pin(Pin(10, "GPIO7", Shape.RECTANGLE, Side.LEFT, PinType.NOT_USED))
pico.add_pin(Pin(11, "GPIO8", Shape.RECTANGLE, Side.LEFT, PinType.NOT_USED))
pico.add_pin(Pin(12, "GPIO9", Shape.RECTANGLE, Side.LEFT, PinType.NOT_USED))
pico.add_pin(Pin(13, "GND", Shape.RECTANGLE, Side.LEFT, PinType.UNUSABLE))
pico.add_pin(Pin(14, "GPIO10", Shape.RECTANGLE, Side.LEFT, PinType.NOT_USED))
pico.add_pin(Pin(15, "GPIO11", Shape.RECTANGLE, Side.LEFT, PinType.NOT_USED))
pico.add_pin(Pin(16, "GPIO12", Shape.RECTANGLE, Side.LEFT, PinType.NOT_USED))
pico.add_pin(Pin(17, "GPIO13", Shape.RECTANGLE, Side.LEFT, PinType.NOT_USED))
pico.add_pin(Pin(18, "GND", Shape.RECTANGLE, Side.LEFT, PinType.UNUSABLE))
pico.add_pin(Pin(19, "GPIO14", Shape.RECTANGLE, Side.LEFT, PinType.NOT_USED))
pico.add_pin(Pin(20, "GPIO15", Shape.RECTANGLE, Side.LEFT, PinType.NOT_USED))

pico.add_pin(Pin(1, "VBUS", Shape.RECTANGLE, Side.RIGHT, PinType.UNUSABLE))
pico.add_pin(Pin(2, "VSYS", Shape.RECTANGLE, Side.RIGHT, PinType.UNUSABLE))
pico.add_pin(Pin(3, "GND", Shape.RECTANGLE, Side.RIGHT, PinType.UNUSABLE))
pico.add_pin(Pin(4, "3V3_EN", Shape.RECTANGLE, Side.RIGHT, PinType.UNUSABLE))
pico.add_pin(Pin(5, "3V3", Shape.RECTANGLE, Side.RIGHT, PinType.UNUSABLE))
pico.add_pin(Pin(6, "ADC_VREF", Shape.RECTANGLE, Side.RIGHT, PinType.UNUSABLE))
pico.add_pin(Pin(7, "GPIO28", Shape.RECTANGLE, Side.RIGHT, PinType.NOT_USED))
pico.add_pin(Pin(8, "GND", Shape.RECTANGLE, Side.RIGHT, PinType.UNUSABLE))
pico.add_pin(Pin(9, "GPIO27", Shape.RECTANGLE, Side.RIGHT, PinType.NOT_USED))
pico.add_pin(Pin(10, "GPIO26", Shape.RECTANGLE, Side.RIGHT, PinType.NOT_USED))
pico.add_pin(Pin(11, "RUN", Shape.RECTANGLE, Side.RIGHT, PinType.UNUSABLE))
pico.add_pin(Pin(12, "GPIO22", Shape.RECTANGLE, Side.RIGHT, PinType.NOT_USED))
pico.add_pin(Pin(13, "GND", Shape.RECTANGLE, Side.RIGHT, PinType.UNUSABLE))
pico.add_pin(Pin(14, "INTn", Shape.RECTANGLE, Side.RIGHT, PinType.UNUSABLE))
pico.add_pin(Pin(15, "RSTn", Shape.RECTANGLE, Side.RIGHT, PinType.UNUSABLE))
pico.add_pin(Pin(16, "MOSI", Shape.RECTANGLE, Side.RIGHT, PinType.UNUSABLE))
pico.add_pin(Pin(17, "SCLK", Shape.RECTANGLE, Side.RIGHT, PinType.UNUSABLE))
pico.add_pin(Pin(18, "GND", Shape.RECTANGLE, Side.RIGHT, PinType.UNUSABLE))
pico.add_pin(Pin(19, "CSn", Shape.RECTANGLE, Side.RIGHT, PinType.UNUSABLE))
pico.add_pin(Pin(20, "MISO", Shape.RECTANGLE, Side.RIGHT, PinType.UNUSABLE))

stepgens = []
for i in range(0, 4):
    stepgens.append(Node(200, 50 * i, 100, 50, "None", 5, f"Stepgen{i}", 2, 2))
    stepgens[-1].pin_y_offs = 5
    stepgens[-1].add_pin(Pin(1, "Step", Shape.RECTANGLE, Side.RIGHT, PinType.NOT_USED))
    stepgens[-1].add_pin(Pin(2, "Dir", Shape.RECTANGLE, Side.RIGHT, PinType.NOT_USED))
    stepgens[-1].add_pin(Pin(1, "Dir", Shape.RECTANGLE, Side.LEFT, PinType.NOT_USED))
    stepgens[-1].add_pin(Pin(2, "Step", Shape.RECTANGLE, Side.LEFT, PinType.NOT_USED))

encoders = []
for i in range(0, 4):
    encoders.append(Node(320, 70 * i, 100, 70, "None", 5, f"Encoder{i}", 3, 3))
    encoders[-1].pin_y_offs = 5
    encoders[-1].add_pin(Pin(1, "A", Shape.RECTANGLE, Side.RIGHT, PinType.NOT_USED))
    encoders[-1].add_pin(Pin(2, "B", Shape.RECTANGLE, Side.RIGHT, PinType.NOT_USED))
    encoders[-1].add_pin(Pin(3, "Z", Shape.RECTANGLE, Side.RIGHT, PinType.NOT_USED))
    encoders[-1].add_pin(Pin(1, "Z", Shape.RECTANGLE, Side.LEFT, PinType.NOT_USED))
    encoders[-1].add_pin(Pin(2, "B", Shape.RECTANGLE, Side.LEFT, PinType.NOT_USED))
    encoders[-1].add_pin(Pin(3, "A", Shape.RECTANGLE, Side.LEFT, PinType.NOT_USED))
