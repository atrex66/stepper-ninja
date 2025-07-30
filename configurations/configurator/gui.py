import tkinter
import pickle
import tkinter.filedialog
import pygame
import numpy as np
from base64 import b64encode, b64decode
import json
import os
from enum import Enum

# Pygame inicializálása
pygame.init()
pygame.font.init()

screen = pygame.display.set_mode((1280, 1000))

curves = []
node_indexes = {}
hint = None

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
                json_files.append(file.split('.')[0])
        
        if not json_files:
            print(f"Nincs JSON fájl a {directory} könyvtárban!")
        
        return json_files
    
    except Exception as e:
        print(f"Hiba történt a fájlok listázása közben: {str(e)}")
        return []


def load_component(json_file_path):
    global node_indexes
    try:
        with open(os.path.join(BASE_DIR, json_file_path + ".json"), 'r') as file:
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
    hide_pin_names = False
    if "HidePinNames" in data.keys():
        hide_pin_names = True if data["HidePinNames"] == 1 else False
    master = True if data["DeviceType"] == "MASTER" else False

    if image_path != "None":
        image = pygame.image.load(os.path.join(BASE_DIR, image_path))
    else:
        image = pygame.Surface((defwidth, defheight))

    if name not in node_indexes.keys():
        node_indexes[name] = 0
    node = Node(200, 350, image.get_width(), image.get_height(), image_path, 5, name, leftPins, rightPins)

    node.hide_pin_names = hide_pin_names
    if "bgcolor" in data.keys():
        node.name_background = data["bgcolor"]
    if "textColor" in data.keys():
        node.text_color = data["textColor"]
    
    node.master = master
    node.pin_y_offs = pinoffsy
    node_indexes[name] += 1
    node.refresh()

    for pin in data["Pins"]:
        Pinpos = pin["pinPos"]
        name = pin["name"]
        shape = Shape.RECTANGLE
        abs_x = None
        abs_y = None
        if "AbsX" in pin:
            abs_x = pin["AbsX"]
            abs_y = pin["AbsY"]
        shape = Shape.RECTANGLE
        size = None
        if "shape" in pin.keys():
            if pin["shape"] == "CIRCLE":
                shape = Shape.CIRCLE
                size = pin["size"]
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
        newpin = Pin(Pinpos, name, shape, pinside, pintype, abs_x, abs_y, size)
        newpin.function = pin['function']
        node.add_pin(newpin)
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


class Orientation(Enum):
    HORIZONTAL = 0
    VERTICAL = 1


class GradientSurface(pygame.Surface):
    def __init__(self, size, start_color, end_color):
        super().__init__(size, flags = pygame.SRCALPHA)
        self.fill(0)
        self.size = size
        self.start_color = start_color
        self.end_color = end_color

    def gradientFill(self, color1:pygame.Color, color2:pygame.Color, orient):
        if orient == Orientation.HORIZONTAL:
            b = pygame.Surface((2,1))
            pygame.draw.line(b, color1, (0,0), (0,0), 1)
            pygame.draw.line(b, color2, (1,0), (1,0), 1)
            b = pygame.transform.smoothscale(b, (self.size[0], self.size[1]))
        elif orient == Orientation.VERTICAL:
            b = pygame.Surface((1,2))
            pygame.draw.line(b, color1, (0,0), (0,0), 1)
            pygame.draw.line(b, color2, (0,1), (0,1), 1)
            b = pygame.transform.smoothscale(b, (self.size[0], self.size[1]))
        else:
            self.fill(0)
            return
        self.blit(b,(0, 0))

    def __getstate__(self):
        state = self.__dict__.copy()  # Másolat a __dict__-ből
        for key, value in state.items():
            if isinstance(value, pygame.Surface):
                # Surface-t nyers pixeladatokká konvertálunk
                pixel_data = pygame.image.tostring(value, "RGBA")  # RGBA formátum használata
                state[key] = {
                    "type": "pygame.Surface",
                    "size": value.get_size(),
                    "pixel_format": "RGBA",
                    "pixel_data": b64encode(pixel_data).decode('ascii')
                }
        return state

    def __setstate__(self, state):
        self.__dict__.update(state)  # Alap attribútumok visszaállítása
        for key, value in self.__dict__.items():
            if isinstance(value, dict) and value.get("type") == "pygame.Surface":
                # Pixeladatokból Surface visszaállítása
                pixel_data = b64decode(value["pixel_data"])
                surface = pygame.image.fromstring(pixel_data, value["size"], value["pixel_format"])
                self.__dict__[key] = surface

class Form:
    def __init__(self, r:pygame.Rect, caption:str, font:pygame.font.Font, gradient:list, font_fg:pygame.color.Color = (0, 0, 0)):
        self.rectangle = r
        self.caption = caption
        self.gradient = gradient
        self.font_fg = font_fg
        self.font = font
        self.titlefontheight = self.font.render("A", False, (0,0,0)).get_height()
        self.surface = pygame.Surface((r.width, r.height))
        self.client_rect = pygame.Rect(r.x + 1, r.y + self.titlefontheight + 2, 0, 0)
        self.refresh()
        self.client_surface = pygame.Surface((self.surface.get_width()-2, self.rectangle.height - self.title.get_height() - 3))
        self.client_rect.width = self.client_surface.get_width()
        self.client_rect.height = self.client_surface.get_width()
        self.draggign = False
        self.offsx = 0
        self.offsy = 0

    def handle_events(self, event:pygame.event.Event):
        if event.type == pygame.MOUSEBUTTONDOWN:
            if self.rectangle.collidepoint(event.pos):
                self.draggign = True
                self.offsx = event.pos[0] - self.rectangle.x
                self.offsy = event.pos[1] - self.rectangle.y
        if event.type == pygame.MOUSEBUTTONUP:
                self.draggign = False
                self.offsx = 0
                self.offsy = 0
        if event.type == pygame.MOUSEMOTION:
            if self.draggign:
                self.rectangle.x = event.pos[0] - self.offsx
                self.rectangle.y = event.pos[1] - self.offsy
                self.refresh()

    def refresh(self):
        text = self.font.render(self.caption, True, self.font_fg)
        self.title = pygame.Surface((self.rectangle.width - 2, self.titlefontheight + 2))
        gradient = pygame.Surface((2, 1))
        pygame.draw.line(gradient, self.gradient[0], (0,0), (0,0))
        pygame.draw.line(gradient, self.gradient[1], (1,0), (1,0))
        self.title = pygame.transform.smoothscale(gradient, (self.title.get_width(), self.title.get_height()))
        self.title.blit(text, (1,1))
        self.surface.blit(self.title, (1,1))
        self.client_rect.x = self.rectangle.x + 1
        self.client_rect.y = self.rectangle.y + self.title.get_height() + 2

    def draw(self, surface:pygame.surface.Surface):
        self.surface.blit(self.client_surface, (1, self.title.get_height() + 2))
        surface.blit(self.surface, (self.rectangle.x, self.rectangle.y))

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
        self.color = [color, color]  # Görbe színe
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
            l2 = []
            for ind, line in enumerate(bezier_points):
                l2.append(line)
                color = self.color[0]
                if ind % 3 != 0:
                    color = self.color[1]
                if len(l2)>1:
                    pygame.draw.line(surface, color, l2[0], l2[1], self.thickness)
                    l2.pop(0)

    def __getstate__(self):
        state = self.__dict__.copy()  # Másolat a __dict__-ből
        for key, value in state.items():
            if isinstance(value, pygame.Surface):
                # Surface-t nyers pixeladatokká konvertálunk
                pixel_data = pygame.image.tostring(value, "RGBA")  # RGBA formátum használata
                state[key] = {
                    "type": "pygame.Surface",
                    "size": value.get_size(),
                    "pixel_format": "RGBA",
                    "pixel_data": b64encode(pixel_data).decode('ascii')
                }
        return state

    def __setstate__(self, state):
        self.__dict__.update(state)  # Alap attribútumok visszaállítása
        for key, value in self.__dict__.items():
            if isinstance(value, dict) and value.get("type") == "pygame.Surface":
                # Pixeladatokból Surface visszaállítása
                pixel_data = b64decode(value["pixel_data"])
                surface = pygame.image.fromstring(pixel_data, value["size"], value["pixel_format"])
                self.__dict__[key] = surface


class Pin:
    def __init__(self, pinNo:int, pinName:str, shape:int, pinSide:int, typus:int, abs_x=None, abs_y=None, size=None):
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
        self.abs_x = abs_x
        self.abs_y = abs_y
        self.size = size
        self.hide_name = False
        self.function = ""
        self.hint_timer = 0
        self.hint_object = None
        self.hint_showed = False
    
    def __getstate__(self):
        state = self.__dict__.copy()  # Másolat a __dict__-ből
        for key, value in state.items():
            if isinstance(value, pygame.Surface):
                # Surface-t nyers pixeladatokká konvertálunk
                pixel_data = pygame.image.tostring(value, "RGBA")  # RGBA formátum használata
                state[key] = {
                    "type": "pygame.Surface",
                    "size": value.get_size(),
                    "pixel_format": "RGBA",
                    "pixel_data": b64encode(pixel_data).decode('ascii')
                }
        return state

    def __setstate__(self, state):
        self.__dict__.update(state)  # Alap attribútumok visszaállítása
        for key, value in self.__dict__.items():
            if isinstance(value, dict) and value.get("type") == "pygame.Surface":
                # Pixeladatokból Surface visszaállítása
                pixel_data = b64decode(value["pixel_data"])
                surface = pygame.image.fromstring(pixel_data, value["size"], value["pixel_format"])
                self.__dict__[key] = surface

    def __str__(self):
        return f"{self.name}:{self.pinNo}"

    def set_metrics(self, width:int, height:int):
        self.rect.width = width
        self.rect.height = height

    def set_curveStart(self, rect:pygame.Rect):
        if self.shape != Shape.CIRCLE:
            self.curveStart = (rect.x + self.rect.x + (self.rect.width / 2), rect.y + self.rect.y + (self.rect.height / 2))
        else:
            self.curveStart = (rect.x + self.rect.x + (self.rect.width / 2) - self.size, rect.y + self.rect.y + (self.rect.height / 2) - self.size)

    def generate_pin(self, color = None):
        self.surface = pygame.Surface((self.rect.width, self.rect.height), pygame.SRCALPHA)
        if color == None:
            if self.typus == PinType.NOT_USED:
                self.color = pygame.Color("white")
            elif self.typus == PinType.UNUSABLE:
                self.color = pygame.Color("red")
            elif self.typus == PinType.INPUT:
                self.color = pygame.Color("green")
            elif self.typus == PinType.OUTPUT:
                self.color = pygame.Color("orange")
        else:
            self.color = color
        if self.shape == Shape.RECTANGLE:
            self.surface.fill(self.color)
        elif self.shape == Shape.CIRCLE:
            pygame.draw.circle(self.surface, self.color, (self.rect.width / 2, self.rect.height / 2), self.size, width=0)
        elif self.shape == Shape.ELLIPSE:
            pygame.draw.ellipse(self.surface, self.color, pygame.Rect(0, 0, self.rect.width, self.rect.height), width=0)

    def draw(self, surface:pygame.Surface):
        global hint
        if self.shape != Shape.CIRCLE:
            r = pygame.Rect(self.parent.main_rectangle.x + self.rect.x, self.parent.main_rectangle.y + self.rect.y, self.rect.width, self.rect.height)
        else:
            r = pygame.Rect(self.parent.main_rectangle.x + self.rect.x - self.size, self.parent.main_rectangle.y + self.rect.y - self.size, self.size * 2, self.size * 2)
        if r.collidepoint(pygame.mouse.get_pos()):
            if self.typus != PinType.UNUSABLE:
                self.generate_pin("green")
        else: 
            self.generate_pin()
        if not self.hide_name:
            if self.pinSide == Side.RIGHT:
                surface.blit(self.text, (self.rect.x - self.text.get_width(), self.rect.y))
            if self.pinSide == Side.LEFT:
                surface.blit(self.text, (self.rect.x + self.rect.width, self.rect.y))
        if self.shape != Shape.CIRCLE:
            surface.blit(self.surface, (self.rect.x, self.rect.y))
        else:
            surface.blit(self.surface, (self.rect.x - self.size, self.rect.y - self.size))


class Node:
    def __init__(self, x:int, y:int, width:int, height:int, image_path:str, margin:int, label:str, leftpins_max:int, rightpins_max:int):
        global node_indexes
        self.path = os.path.join(BASE_DIR, image_path)
        self.image_path = image_path
        self.margin = margin
        self.name = f"{label}{node_indexes[label]}"
        self.index = node_indexes[label]
        self.x = x
        self.y = y
        self.master = False
        self.width = width
        self.height = height
        self.name_background = "white"
        self.text_color = "black"
        self.label = FONT.render(self.name , 1, self.text_color)
        self.pin_y_offs = 0
        self.leftpins = []
        self.rigthpins = []
        self.max_leftpins = leftpins_max
        self.max_rightpins = rightpins_max
        self.noimage = 0
        self.selectedPin = None
        self.lastkkom = 0
        self.main_rectangle = pygame.Rect(0,0,0,0)
        self.dragging = False
        self.image = None
        if os.path.exists(self.path):
            self.image = pygame.image.load(os.path.join(BASE_DIR, self.image_path))
            self.width = self.image.get_width()
            self.height = self.image.get_height()
        self.refresh(self.width, self.height)
        self.offsetx = 0
        self.offsety = 0
        self.hint_timer = 0
        self.hint_object = None
        self.hint_showed = False
        self.hide_pin_names = False

    def __getstate__(self):
        state = self.__dict__.copy()  # Másolat a __dict__-ből
        for key, value in state.items():
            if isinstance(value, pygame.Surface):
                # Surface-t nyers pixeladatokká konvertálunk
                pixel_data = pygame.image.tostring(value, "RGBA")  # RGBA formátum használata
                state[key] = {
                    "type": "pygame.Surface",
                    "size": value.get_size(),
                    "pixel_format": "RGBA",
                    "pixel_data": b64encode(pixel_data).decode('ascii')
                }
        return state

    def __setstate__(self, state):
        self.__dict__.update(state)  # Alap attribútumok visszaállítása
        for key, value in self.__dict__.items():
            if isinstance(value, dict) and value.get("type") == "pygame.Surface":
                # Pixeladatokból Surface visszaállítása
                pixel_data = b64decode(value["pixel_data"])
                surface = pygame.image.fromstring(pixel_data, value["size"], value["pixel_format"])
                self.__dict__[key] = surface

    def set_pin_y_offset(self, offs:int):
        self.pin_y_offs = offs

    def add_pin(self, pin:Pin):
        pin.hide_name = self.hide_pin_names
        if pin.shape != Shape.CIRCLE:
            pin.set_metrics(15, 10)
        else:
            pin.set_metrics(pin.size * 2, pin.size * 2)
        pin.generate_pin()
        pin.parent = self
        dormantspace = self.label.get_height()
        clientheight = self.main_rectangle.height - dormantspace
        if pin.pinSide == Side.LEFT:
            if pin.abs_x == None:
                if self.max_leftpins < pin.pinNo:
                    return
                pin.rect.y = ((clientheight / self.max_leftpins) * (pin.pinNo - 1)) + self.pin_y_offs + dormantspace
                pin.rect.x = 0
                self.leftpins.append(pin)
            else:
                pin.rect.x = pin.abs_x
                pin.rect.y = pin.abs_y + dormantspace
                self.leftpins.append(pin)
        if pin.pinSide == Side.RIGHT:
            if pin.abs_x == None:
                if self.max_rightpins < pin.pinNo:
                    return
                pin.rect.y = ((clientheight / self.max_rightpins) * (pin.pinNo - 1)) + self.pin_y_offs + dormantspace
                pin.rect.x = self.main_rectangle.width - pin.rect.width
                self.rigthpins.append(pin)
            else:
                pin.rect.x = self.main_rectangle.width - pin.abs_x
                pin.rect.y = pin.abs_y + dormantspace
                self.leftpins.append(pin)

    def refresh(self, width=0, height=0):
        self.surface = pygame.Surface((self.width , self.height + self.label.get_height()), pygame.SRCALPHA)
        self.surface.fill("black")
        self.noimage = 0
        if self.image:        
            self.surface.blit(self.image, (0, self.label.get_height()))
        self.label = FONT.render(self.name , 1, self.text_color)
        pygame.draw.rect(self.surface, self.name_background, pygame.Rect(0, 0, self.main_rectangle.width, self.label.get_height()), width=0)
        self.surface.blit(self.label, (1, 1))
        if not self.dragging:
            self.main_rectangle = pygame.Rect(self.x, self.y, self.surface.get_width(), self.surface.get_height())

    def handle_events(self, e:pygame.event.Event):
        global curves

        if e.type == pygame.KEYDOWN:
            if e.key == pygame.K_ESCAPE:
                if self.selectedPin != None:
                    self.selectedPin = None
                    if curves[-1].Pin1 == None:
                        curves.pop()

        x, y = pygame.mouse.get_pos()
        if e.type == pygame.MOUSEMOTION:
            if self.dragging:
                self.main_rectangle.x = e.pos[0] - self.offsx
                self.main_rectangle.y = e.pos[1] - self.offsy
                self.refresh()
                for pin in self.leftpins:
                    pin.set_curveStart(self.main_rectangle)
                for pin in self.rigthpins:
                    pin.set_curveStart(self.main_rectangle)
                return

        if not self.main_rectangle.collidepoint(x, y):
            self.hint_timer = 0
            self.hint_showed = False
            return

        if e.type == pygame.MOUSEBUTTONDOWN:
            if e.button != 1:
                return
            self.selectedPin = None
            for pin in self.leftpins + self.rigthpins:
                if pin.shape != Shape.CIRCLE:
                    evrect = pygame.Rect(self.main_rectangle.x + pin.rect.x, self.main_rectangle.y + pin.rect.y, pin.rect.width, pin.rect.height)
                else:
                    evrect = pygame.Rect(self.main_rectangle.x + pin.rect.x - pin.size, self.main_rectangle.y + pin.rect.y - pin.size, pin.rect.width, pin.rect.height)
                if evrect.collidepoint(x, y):
                    pin.set_curveStart(self.main_rectangle)
                    if pin != self.selectedPin:
                        if len(curves) > 0:
                            if curves[-1].Pin1 == None:
                                curves[-1].Pin1 = pin
                            else:
                                curves.append(Curves(pin, None))
                            self.selectedPin = pin
                        else:
                            curves.append(Curves(pin, None))
                    self.selectedPin = pin

            if self.selectedPin == None:
                self.dragging = True
                self.offsx = e.pos[0] - self.main_rectangle.x
                self.offsy = e.pos[1] - self.main_rectangle.y
        if e.type == pygame.MOUSEBUTTONUP:
            self.dragging = False

    def draw(self, surface:pygame.Surface):
        for pin in self.leftpins + self.rigthpins:
            if pin.parent == None:
                pin.parent = self
            pin.draw(self.surface)
        surface.blit(self.surface, (self.main_rectangle.x, self.main_rectangle.y))
        pygame.draw.rect(surface, "red", self.main_rectangle, 1)

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
        if not self.rectangle.collidepoint(pygame.mouse.get_pos()):
            return
        if event.type == pygame.MOUSEBUTTONDOWN:
            if not pygame.mouse.get_pressed(3)[0]:
                return
            for ind, rect in enumerate(self.rectangles):
                crect = pygame.Rect(self.rectangle.x + rect.x, self.rectangle.y + rect.y, rect.width, rect.height)
                if crect.collidepoint(pygame.mouse.get_pos()):
                    self.selected = ind
                    self.update()
                    return
                else:
                    self.selected = None
                    self.update()

    def draw(self, surf:pygame.Surface):
        surf.blit(self.surface, (self.rectangle.x, self.rectangle.y))

class Button:
    def __init__(self, r:pygame.Rect, text:str, name):
        self.rectangle = r
        self.text = text
        self.id = name
        self.pressed = False
        #self.surface = pygame.Surface((r.width, r.height))
        self.surface = GradientSurface((r.width, r.height), "lightblue", "darkgray")
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
            self.surface.gradientFill("blue", "lightgray", orient=Orientation.VERTICAL)
        else:
            self.surface.gradientFill("lightblue", "darkgray", orient=Orientation.VERTICAL)
        text = BIG_FONT.render(self.text, True, pygame.Color("black"))
        x = (self.rectangle.width / 2) - (text.get_width() / 2)
        y = (self.rectangle.height / 2) - (text.get_height() / 2)
        pygame.draw.rect(self.surface, "black", (0, 0, self.rectangle.width, self.rectangle.height), width=1, border_radius=3)
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


def save_objects(filename, objects):
    with open(os.path.join(BASE_DIR, filename), "wb") as f:
        pickle.dump(objects, f)


def load_objects(filename):
    pathname = os.path.join(BASE_DIR, filename)
    if os.path.exists(pathname):
        with open(pathname, "rb") as f:
            return pickle.load(f)
