import pygame
import itertools
from gui import *

screen = pygame.display.set_mode((1280, 1000))
pygame.display.set_caption("Stepper-Ninja Configurator !ALPHA!")
clock = pygame.time.Clock()

nodes = []

# 16 VGA szín neve és RGB értékei
vga_colors = {
    "black": (0, 0, 0),
    "navy": (0, 0, 128),
    "green": (0, 128, 0),
    "teal": (0, 128, 128),
    "maroon": (128, 0, 0),
    "purple": (128, 0, 128),
    "olive": (128, 128, 0),
    "violet": "violet",
    "goldenrod": "goldenrod",
    "blue": (0, 0, 255),
    "lime": (0, 255, 0),
    "aqua": (0, 255, 255),
    "red": (255, 0, 0),
    "fuchsia": (255, 0, 255),
    "yellow": (255, 255, 0),
    "white": (255, 255, 255)
}

# Összes színpár generálása (kivéve azonos színek)
color_names = list(vga_colors.keys())
color_pairs = [[c1, c2] for c1, c2 in itertools.permutations(color_names, 2)]

comp_dir = "components"
comps = list_json_files(comp_dir)
components = []
pinek = []
components.append(ListBox(pygame.Rect(0, 0, 200, 800), comps, "components"))
components.append(ListBox(pygame.Rect(screen.get_width() - 200, 0, 200, 800), pinek, "connections"))
components[-1].font = FONT

external = 0

def button_callback(id):
    global curves, nodes, external
    if id == "ADDCOMP":
        nodes.append(load_component(os.path.join(comp_dir, components[0].get_selected())))
    if id == "DELLAST":
        curves.pop()
    if id == "DELCONN":
        if components[1].selected != 0:
            curves.pop(components[-1].selected)
    if id == "BUILDINSTALL":
        external = 1
        filename = prompt_file()
        print(filename)
        external = 0

buttons = []
buttons.append(Button(pygame.Rect(0, 949, 200, 50), "Add comp", name="ADDCOMP"))
buttons[-1].callback = button_callback
buttons.append(Button(pygame.Rect(205, 949, 200, 50), "Del comp", name="DELLAST"))
buttons[-1].callback = button_callback
buttons.append(Button(pygame.Rect(410, 949, 200, 50), "Build+Install", name="BUILDINSTALL"))
buttons[-1].callback = button_callback
buttons.append(Button(pygame.Rect(615, 949, 200, 50), "Del conn", name="DELCONN"))
buttons[-1].callback = button_callback

# forms = Form(pygame.Rect(250,250,200,200),"test form", FONT, ["lightblue","darkgray"], "black")

# Fő ciklus
running = True
while running:
    screen.fill("gray70")

    for event in pygame.event.get():
        if event.type == pygame.QUIT:
            running = False
        if not external:
            for node in nodes:
                node.handle_events(event)
            for comp in components:
                comp.handle_event(event)
            for button in buttons:
                button.handle_event(event)

    for node in nodes:
        node.draw(screen)

    for button in buttons:
        button.draw(screen)

    k = []
    pinek = []
    for i, curve in enumerate(curves):
        t = 2
        if components[1].selected == i:
            t = 5
        if curve.Pin1 == None:
            k.append(BezierCurve(curve.Pin0.curveStart, pygame.mouse.get_pos(), offset=20, thickness=t, color=(255, 255, 0)))
            k[-1].color = color_pairs[i]
        else:
            k.append(BezierCurve(curve.Pin0.curveStart, curve.Pin1.curveStart, offset=20, thickness=t, color=(255, 255, 0)))
            pinek.append(f"{curve.Pin0.parent.name}-{curve.Pin0.name} -> {curve.Pin1.parent.name}-{curve.Pin1.name}")
            k[-1].color = color_pairs[i]
        
        k[-1].set_dynamic_offset()
        k[-1].draw(screen)

    for comp in components:
        if comp.name == "connections":
            comp.lines = pinek
            comp.update()
        comp.draw(screen)

    # forms.draw(screen)

    pygame.display.flip()
    clock.tick(60)

pygame.quit()