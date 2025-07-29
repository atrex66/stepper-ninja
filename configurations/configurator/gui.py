import tkinter
import tkinter.filedialog
import pygame
import numpy as np
import json
import os
from enum import Enum

# Pygame inicializálása
pygame.init()
pygame.font.init()

curves = []
node_indexes = {}

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
BIG_FONT = pygame.font.SysFont("comicsans", 32)


def draw_text(screen, x, y, text):
    text = FONT.render(text, True, "white")
    screen.blit(text, (x, y))

def list_json_files(directory):
    try:
        # Ellenőrizzük, hogy a könyvtár létezik-e
        dirs = os.path.join(BASE_DIR, directory)
        if not os.path.isdir(dirs):
            print(f"Hiba: A {directory} könyvtár nem létezik!")
            return []
        
        # Lista a JSON fájlok tárolására
        json_files = []
        
        # Végigiterálunk a könyvtár tartalmán
        for file in os.listdir(dirs):
            # Csak a .json kiterjesztésű fájlokat vesszük fel
            if file.endswith('.json'):
                json_files.append(file)
        
        if not json_files:
            print(f"Nincs JSON fájl a {directory} könyvtárban!")
        
        return json_files
    
    except Exception as e:
        print(f"Hiba történt a fájlok listázása közben: {str(e)}")
        return []


def load_component(json_file_path):
    global node_indexes
    try:
        with open(os.path.join(BASE_DIR, json_file_path), 'r') as file:
            data = json.load(file)
        #return data
    except FileNotFoundError:
        print(f"Error: {json_file_path} file not found!")
        return None
    except json.JSONDecodeError:
        print("Error: Illegal json file, file demaged?")
        return None
    except Exception as e:
        print(f"Error: error while loading json file {str(e)}")
        return None
    name = data["Name"]
    image_path = data["Image"]
    leftPins = data["LeftPins"]
    rightPins = data["RightPins"]
    defwidth = data["DefaultWidth"]
    defheight = data["DefaultHeight"]
    pinoffsy = data["PinOffsetY"]

    if image_path != "None":
        image = pygame.image.load(os.path.join(BASE_DIR, image_path))
    else:
        image = pygame.Surface((defwidth, defheight))

    if name not in node_indexes.keys():
        node_indexes[name] = 0
    node = Node(200, 350, image.get_width(), image.get_height(), image_path, 5, name, leftPins, rightPins)
    node.pin_y_offs = pinoffsy
    node_indexes[name] += 1

    for pin in data["Pins"]:
        Pinpos = pin["pinPos"]
        name = pin["name"]
        shape = Shape.RECTANGLE
        if "shape" in pin.keys():
            if pin["shape"] == "RECTANGLE":
                shape = Shape.RECTANGLE
            elif pin["shape"] == "CIRCLE":
                shape = Shape.CIRCLE
            elif pin["shape"] == "ELLIPSE":
                shape = Shape.ELLIPSE
        if pin["side"] == "LEFT":
            pinside = Side.LEFT
        else:
            pinside = Side.RIGHT
        if pin["type"] == "USABLE":
            pintype = PinType.NOT_USED
        elif pin["type"] == "UNUSABLE":
            pintype = PinType.UNUSABLE
        node.add_pin(Pin(Pinpos, name, shape, pinside, pintype))
    return node

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

    def __str__(self):
        if self.Pin1:
            return f"{self.Pin0.name}:{self.Pin1.Name}"
        else:
            return f"{self.Pin0.name}:None"

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
        bezier_points = self.calculate_cubic_bezier(steps = 20)
        if len(bezier_points) > 1:
            pygame.draw.lines(surface, self.color, False, bezier_points, self.thickness)


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
    
    def __str__(self):
        return f"{self.name}:{self.pinNo}"

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
        global node_indexes
        self.path = os.path.join(BASE_DIR, image_path)
        self.image_path = image_path
        self.margin = margin
        self.name = f"{label}{node_indexes[label]}"
        self.label = FONT.render(self.name , 1, "white")
        self.index = node_indexes[label]
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

    def handle_events(self, e:pygame.event.Event):
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
                    curves.pop()

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
            if not pygame.mouse.get_pressed(3)[0]:
                return
            self.selectedPin = None
            for pin in self.leftpins + self.rigthpins:
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
        for pin in self.leftpins + self.rigthpins:
            pin.draw(self.surface)
        surface.blit(self.surface, (self.main_rectangle.x, self.main_rectangle.y))


class ListBox:
    def __init__(self, r:pygame.Rect, lines:list, name):
        self.rectangle = r
        self.name = name
        self.lines = lines
        self.selected = 0
        self.margin = 5
        self.padding = 2
        self.font = BIG_FONT
        self.rectangles = []
        self.surface = pygame.Surface((self.rectangle.width, self.rectangle.height))
        self.update()

    def update(self):
        self.surface.fill(0)
        self.rectangles = []
        pygame.draw.rect(self.surface, "Yellow", self.rectangle, 1, 3)
        hght = self.font.render("A", True, "White").get_height()
        for index, line in enumerate(self.lines):
            text = self.font.render(line, True, ("white"))
            x = self.padding
            y = self.margin + (index * (hght + self.padding))
            width = self.rectangle.width - self.padding * 2
            height = hght + self.padding
            self.rectangles.append(pygame.Rect(x, y, width, height))
            if self.selected == index:
                pygame.draw.rect(self.surface, "gray", self.rectangles[-1], width=0, border_radius=1)
            self.surface.blit(text, (self.margin, self.margin + (index * (hght + self.padding))))

    def get_selected(self):
        return self.lines[self.selected]

    def handle_event(self, event:pygame.event.Event):
        if event.type == pygame.MOUSEBUTTONDOWN:
            if not pygame.mouse.get_pressed(3)[0]:
                return
            for ind, rect in enumerate(self.rectangles):
                crect = pygame.Rect(self.rectangle.x + rect.x, self.rectangle.y + rect.y, rect.width, rect.height)
                if crect.collidepoint(pygame.mouse.get_pos()):
                    self.selected = ind
                    self.update()

    def draw(self, surf:pygame.Surface):
        surf.blit(self.surface, (self.rectangle.x, self.rectangle.y))

class Button:
    def __init__(self, r:pygame.Rect, text:str, name):
        self.rectangle = r
        self.text = text
        self.id = name
        self.pressed = False
        self.surface = pygame.Surface((r.width, r.height))
        self.callback = self.handle
        self.update()

    def handle(self, text, obj):
        print(text, obj)
        return

    def handle_event(self, event:pygame.event.Event):
        if not self.rectangle.collidepoint(pygame.mouse.get_pos()):
            if self.pressed:
                self.update()
            self.pressed = False
            return
        if not self.pressed:
            if event.type == pygame.MOUSEBUTTONDOWN:
                self.pressed = True
                self.update()
                self.callback(self.id)
        else:
            if event.type == pygame.MOUSEBUTTONUP:
                self.pressed = False
                self.update()

    def update(self):
        if self.pressed:
            self.surface.fill("azure3")
        else:
            self.surface.fill("azure4")
        pygame.draw.rect(self.surface, "darkolivegreen2", self.rectangle, width=1, border_radius=3)
        text = BIG_FONT.render(self.text, True, pygame.Color("black"))
        x = (self.rectangle.width / 2) - (text.get_width() / 2)
        y = (self.rectangle.height / 2) - (text.get_height() / 2)
        self.surface.blit(text, (x, y))
    
    def draw(self, surface:pygame.Surface):
        surface.blit(self.surface, (self.rectangle.x, self.rectangle.y))


def prompt_file():
    """Create a Tk file dialog and cleanup when finished"""
    top = tkinter.Tk()
    # top.withdraw()  # hide window
    file_name = tkinter.filedialog.askopenfilename(initialdir=os.path.join(BASE_DIR, ".."),parent=top)
    top.destroy()
    return file_name
