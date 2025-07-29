import pygame
import json
import os
import numpy as np
from enum import Enum

# Pygame inicializálása
pygame.init()
pygame.font.init()
screen = pygame.display.set_mode((1680, 1050))
clock = pygame.time.Clock()
curves = []

# Színek
WHITE = (255, 255, 255)
GRAY = (50, 50, 50)
LIGHT_GRAY = (150, 150, 150)
BLUE = (100, 150, 255)
HOVER_COLOR = (120, 170, 255)
BLACK = (0, 0, 0, 255)
CONNECTION_COLOR = (255, 255, 255)
YELLOW = (239, 242, 29)

# Futási könyvtár elérése
BASE_DIR = os.path.dirname(os.path.abspath(__file__))
FONT = pygame.font.SysFont("comicsans", 16)


def load_component(json_file_path):
    """
    Betölti a Raspberry Pi Pico pinek adatait egy JSON fájlból.
    
    Args:
        json_file_path (str): A JSON fájl elérési útja
        
    Returns:
        dict: A pinek adatai szótár formátumban
        None: Ha hiba történik a fájl betöltése közben
    """
    try:
        with open(os.path.join(BASE_DIR, json_file_path), 'r') as file:
            data = json.load(file)
        return data
    except FileNotFoundError:
        print(f"Error: {json_file_path} file not found!")
        return None
    except json.JSONDecodeError:
        print("Error: Illegal json file, file demaged?")
        return None
    except Exception as e:
        print(f"Error: error while loading json file {str(e)}")
        return None


class PinType(Enum):
    NOT_USED = 0
    INPUT = 1
    OUTPUT = 2
    UNUSABLE = 3


class Shape(Enum):
    RECTANGLE = 0
    CIRCLE = 1
    ELLIPSE = 2


class Side(Enum):
    LEFT = 0
    RIGHT = 1


class Curves:
    def __init__(self, p0, p1):
        self.Pin0 = p0
        self.Pin1 = p1

class BezierCurve:
    def __init__(self, p0, p3, offset=200, thickness=5, color=(255, 255, 0)):
        self.p0 = p0  # Kiinduló pont
        self.p3 = p3  # Érkezési pont
        self.offset = offset  # Vezérlőpontok távolsága
        self.offset_step = 10  # Lépésköz az offset változtatásához
        self.thickness = thickness  # Görbe vastagsága
        self.color = color  # Görbe színe
        self.control_point_color = (255, 255, 255)  # Vezérlőpontok színe

    def calculate_cubic_bezier(self, steps=100):
        """Köbös Bézier-görbe pontjainak kiszámítása."""
        p1, p2 = self.calculate_control_points()
        points = []
        for t in np.arange(0, 1.01, 1.0 / steps):
            x = (1-t)**3 * self.p0[0] + 3*(1-t)**2*t * p1[0] + 3*(1-t)*t**2 * p2[0] + t**3 * self.p3[0]
            y = (1-t)**3 * self.p0[1] + 3*(1-t)**2*t * p1[1] + 3*(1-t)*t**2 * p2[1] + t**3 * self.p3[1]
            points.append((int(x), int(y)))
        return points

    def calculate_control_points(self):
        """Vezérlőpontok kiszámítása vízszintes induláshoz és érkezéshez."""
        if self.p3[0] >= self.p0[0]:
            p1 = (self.p0[0] + self.offset, self.p0[1])  # P1 jobbra P0-tól
            p2 = (self.p3[0] - self.offset, self.p3[1])  # P2 balra P3-tól
        else:
            p1 = (self.p0[0] - self.offset, self.p0[1])  # P1 balra P0-tól
            p2 = (self.p3[0] + self.offset, self.p3[1])  # P2 jobbra P3-tól
        return p1, p2

    def set_dynamic_offset(self):
        self.offset = max(50, abs(self.p3[0] - self.p0[0]) * 0.3)

    def update_offset(self, increase=True):
        """Offset növelése vagy csökkentése."""
        if increase:
            self.offset += self.offset_step
        else:
            self.offset = max(10, self.offset - self.offset_step)  # Minimum 10

    def draw(self, surface):
        """Bézier-görbe és vezérlőpontok rajzolása."""
        # Végpontok és vezérlőpontok kirajzolása
        #pygame.draw.circle(surface, self.control_point_color, self.p0, 5)
        #pygame.draw.circle(surface, self.control_point_color, self.p3, 5)
        p1, p2 = self.calculate_control_points()
        #pygame.draw.circle(surface, self.control_point_color, p1, 5)
        #pygame.draw.circle(surface, self.control_point_color, p2, 5)

        # Bézier-görbe rajzolása
        bezier_points = self.calculate_cubic_bezier()
        if len(bezier_points) > 1:
            pygame.draw.lines(surface, self.color, False, bezier_points, self.thickness)

        # Offset érték kiírása
        #font = pygame.font.Font(None, 36)
        #text = font.render(f"Offset: {self.offset}", True, self.control_point_color)
        #surface.blit(text, (10, 10))


class Pin:
    def __init__(self, pinNo:int, pinName:str, shape:int, pinSide:int, typus:int):
        self.pinNo = pinNo
        self.name = pinName
        self.pinSide = pinSide
        self.shape = shape
        self.rect = pygame.Rect(0, 0, 0, 0)
        self.surface = None
        self.typus = typus
        self.color = (0,0,0)
        self.text = FONT.render(self.name, True, pygame.Color("white"), pygame.Color("black"))
        self.curveStart = None
        self.parent = None
    
    def set_metrics(self, width:int, height:int):
        self.rect.width = width
        self.rect.height = height

    def set_curveStart(self, rect:pygame.Rect):
        self.curveStart = (rect.x + self.rect.x + (self.rect.width / 2), rect.y + self.rect.y + (self.rect.height / 2))

    def generate_pin(self):
        self.surface = pygame.Surface((self.rect.width, self.rect.height), pygame.SRCALPHA)
        if self.typus == PinType.NOT_USED:
            self.color = pygame.Color("white")
        elif self.typus == PinType.UNUSABLE:
            self.color = pygame.Color("red")
        elif self.typus == PinType.INPUT:
            self.color = pygame.Color("green")
        elif self.typus == PinType.OUTPUT:
            self.color = pygame.Color("orange")

        if self.shape == Shape.RECTANGLE:
            self.surface.fill(self.color)
        elif self.shape == Shape.CIRCLE:
            pygame.draw.circle(self.surface, self.color, (self.rect.width / 2, self.rect.height / 2), self.rect.width / 2, width=0)
        elif self.shape == Shape.ELLIPSE:
            pygame.draw.ellipse(self.surface, self.color, pygame.Rect(0, 0, self.rect.width, self.rect.height), width=0)

    def draw(self, surface:pygame.Surface):
        if self.pinSide == Side.RIGHT:
            surface.blit(self.text, (self.rect.x - self.text.get_width(), self.rect.y))
        if self.pinSide == Side.LEFT:
            surface.blit(self.text, (self.rect.x + self.rect.width, self.rect.y))
        surface.blit(self.surface, (self.rect.x, self.rect.y))


class Node:
    def __init__(self, x:int, y:int, width:int, height:int, image_path:str, margin:int, label:str, leftpins_max:int, rightpins_max:int):
        self.path = os.path.join(BASE_DIR, image_path)
        self.image_path = image_path
        self.margin = margin
        self.label = FONT.render(label, 1, "white")
        self.name = label
        self.x = x
        self.y = y
        self.pin_y_offs = 0
        self.leftpins = []
        self.rigthpins = []
        self.max_leftpins = leftpins_max
        self.max_rightpins = rightpins_max
        self.noimage = 0
        self.selectedPin = None
        self.lastkkom = 0
        self.refresh(width, height)

    def set_pin_y_offset(self, offs:int):
        self.pin_y_offs = offs

    def add_pin(self, pin:Pin):
        pin.set_metrics(15, 10)
        pin.generate_pin()
        pin.parent = self.name
        dormantspace = self.label.get_height()
        clientheight = self.main_rectangle.height - dormantspace
        if pin.pinSide == Side.LEFT:
            if self.max_leftpins < pin.pinNo:
                return
            pin.rect.y = ((clientheight / self.max_leftpins) * (pin.pinNo - 1)) + self.pin_y_offs + dormantspace
            pin.rect.x = 0
            self.leftpins.append(pin)
        if pin.pinSide == Side.RIGHT:
            if self.max_rightpins < pin.pinNo:
                return
            pin.rect.y = ((clientheight / self.max_rightpins) * (pin.pinNo - 1)) + self.pin_y_offs + dormantspace
            pin.rect.x = self.main_rectangle.width - pin.rect.width
            self.rigthpins.append(pin)

    def refresh(self, width=0, height=0):
        if os.path.exists(self.path):
            self.image = pygame.image.load(os.path.join(BASE_DIR, self.image_path))
            self.width = self.image.get_width()
            self.height = self.image.get_height() + self.label.get_height()
            self.surface = pygame.Surface((self.width , self.height), pygame.SRCALPHA)
            self.noimage = 0
        else:
            self.image = pygame.Surface((width - self.margin, height - self.margin), pygame.SRCALPHA)
            self.width = width
            self.height = height
            self.surface = pygame.Surface((self.width, self.height + self.label.get_height()), pygame.SRCALPHA)
            self.surface.fill((0,0,0,255))
            self.noimage = 1
        
        self.surface.blit(self.image, (0, self.label.get_height()))
        self.surface.blit(self.label, (0, 0))
        self.main_rectangle = pygame.Rect(self.x, self.y, self.width, self.height)

    def handle_events(self, e:pygame.event):
        global curves
        x, y = pygame.mouse.get_pos()

        if e.type == pygame.KEYDOWN:
            self.lastkkom = e.type
            if e.key == pygame.K_ESCAPE:
                if len(curves) > 0:
                    if curves[-1].Pin1 == None:
                        curves = curves[:-1]
            if e.key == pygame.K_DELETE:
                if len(curves) > 0:
                    print(len(curves))
                    curves = curves[:-1]

        if not self.main_rectangle.collidepoint(x, y):
            return
        if e.type == pygame.MOUSEMOTION:
            xr, yr = pygame.mouse.get_rel()
            if pygame.mouse.get_pressed(3)[0]:
                self.main_rectangle.move_ip(xr, yr)
                for pin in self.leftpins:
                    pin.set_curveStart(self.main_rectangle)
                for pin in self.rigthpins:
                    pin.set_curveStart(self.main_rectangle)
        if e.type == pygame.MOUSEBUTTONDOWN:
            self.selectedPin = None
            for pin in self.leftpins:
                evrect = pygame.Rect(self.main_rectangle.x + pin.rect.x, self.main_rectangle.y + pin.rect.y, pin.rect.width, pin.rect.height)
                if evrect.collidepoint(x, y):
                    pin.set_curveStart(self.main_rectangle)
                    if pin != self.selectedPin:
                        if len(curves) > 0:
                            if curves[-1].Pin1 == None:
                                curves[-1].Pin1 = pin
                            else:
                                curves.append(Curves(pin, None))
                        else:
                            curves.append(Curves(pin, None))
                    self.selectedPin = pin
            for pin in self.rigthpins:
                evrect = pygame.Rect(self.main_rectangle.x + pin.rect.x, self.main_rectangle.y + pin.rect.y, pin.rect.width, pin.rect.height)
                if evrect.collidepoint(x, y):
                    pin.set_curveStart(self.main_rectangle)
                    if pin != self.selectedPin:
                        if len(curves) > 0:
                            if curves[-1].Pin1 == None:
                                curves[-1].Pin1 = pin
                            else:
                                curves.append(Curves(pin, None))
                        else:
                            curves.append(Curves(pin, None))
                    self.selectedPin = pin

    def draw(self, surface:pygame.Surface):
        for pin in self.leftpins:
            pin.draw(self.surface)
        for pin in self.rigthpins:
            pin.draw(self.surface)
        surface.blit(self.surface, (self.main_rectangle.x, self.main_rectangle.y))


pico1 = load_component("pico.json")

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
    stepgens.append(Node(0, 50 * i, 100, 50, "None", 5, f"Stepgen{i}", 2, 2))
    stepgens[-1].pin_y_offs = 5
    stepgens[-1].add_pin(Pin(1, "Step", Shape.RECTANGLE, Side.RIGHT, PinType.NOT_USED))
    stepgens[-1].add_pin(Pin(2, "Dir", Shape.RECTANGLE, Side.RIGHT, PinType.NOT_USED))
    stepgens[-1].add_pin(Pin(1, "Dir", Shape.RECTANGLE, Side.LEFT, PinType.NOT_USED))
    stepgens[-1].add_pin(Pin(2, "Step", Shape.RECTANGLE, Side.LEFT, PinType.NOT_USED))

encoders = []
for i in range(0, 4):
    encoders.append(Node(120, 70 * i, 100, 70, "None", 5, f"Encoder{i}", 3, 3))
    encoders[-1].pin_y_offs = 5
    encoders[-1].add_pin(Pin(1, "A", Shape.RECTANGLE, Side.RIGHT, PinType.NOT_USED))
    encoders[-1].add_pin(Pin(2, "B", Shape.RECTANGLE, Side.RIGHT, PinType.NOT_USED))
    encoders[-1].add_pin(Pin(3, "Z", Shape.RECTANGLE, Side.RIGHT, PinType.NOT_USED))
    encoders[-1].add_pin(Pin(1, "Z", Shape.RECTANGLE, Side.LEFT, PinType.NOT_USED))
    encoders[-1].add_pin(Pin(2, "B", Shape.RECTANGLE, Side.LEFT, PinType.NOT_USED))
    encoders[-1].add_pin(Pin(3, "A", Shape.RECTANGLE, Side.LEFT, PinType.NOT_USED))

def draw_text(x, y, text):
    text = FONT.render(text, True, "white")
    screen.blit(text, (x, y))


# Fő ciklus
running = True
while running:
    screen.fill(GRAY)
    
    for event in pygame.event.get():
        if event.type == pygame.QUIT:
            running = False
        pico.handle_events(event)
        for stepgen in stepgens:
            stepgen.handle_events(event)
        for encoder in encoders:
            encoder.handle_events(event)

    for stepgen in stepgens:
        stepgen.draw(screen)

    for encoder in encoders:
        encoder.draw(screen)

    pico.draw(screen)

    k = []
    i = 0
    for curve in curves:
        if curve.Pin1 == None:
            k.append(BezierCurve(curve.Pin0.curveStart, pygame.mouse.get_pos(), offset=100, thickness=2, color=(255,255,0)))    
        else:
            k.append(BezierCurve(curve.Pin0.curveStart, curve.Pin1.curveStart, offset=100, thickness=2, color=(255,255,0)))
            text = f"{curve.Pin0.parent}-{curve.Pin0.name} -> {curve.Pin1.parent}-{curve.Pin1.name}"
            draw_text(0, i * 20, text)
            i+=1
        k[-1].set_dynamic_offset()
        k[-1].draw(screen)

    pygame.display.flip()
    clock.tick(60)

pygame.quit()