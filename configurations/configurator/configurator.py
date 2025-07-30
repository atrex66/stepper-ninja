import pygame
import itertools
from gui import *

pygame.display.set_caption("Stepper-Ninja Configurator !ALPHA!")
clock = pygame.time.Clock()

nodes = []

# 16 VGA szín neve és RGB értékei
vga_colors = {
    "green": (0, 128, 0),
    "teal": (0, 128, 128),
    "maroon": (128, 0, 0),
    "purple": (128, 0, 128),
    "olive": (128, 128, 0),
    "navy": (0, 0, 128),
    "violet": "violet",
    "goldenrod": "goldenrod",
    "blue": (0, 0, 255),
    "lime": (0, 255, 0),
    "aqua": (0, 255, 255),
    "red": (255, 0, 0),
    "fuchsia": (255, 0, 255),
    "yellow": (255, 255, 0),
    "black": (0, 0, 0),
    "white": (255, 255, 255)
}

# Összes színpár generálása (kivéve azonos színek)
color_names = list(vga_colors.keys())
color_pairs = [[c1, c2] for c1, c2 in itertools.permutations(color_names, 2)]

comp_dir = "components"
comps = list_json_files(comp_dir)
components = {}
pinek = []
components['components'] = ListBox(pygame.Rect(0, 0, 200, 800), comps, "components")
components['connections'] = ListBox(pygame.Rect(screen.get_width() - 200, 0, 200, 800), pinek, "connections")
components['connections'].font = FONT

external = 0

def button_callback(id):
    global curves, nodes, external
    if id == "ADDCOMP":
        nodename = components['components'].get_selected()
        if nodename:
            nodes.append(load_component(os.path.join(comp_dir, nodename)))
    if id == "DELLAST":
        curves.pop()
    if id == "DELCONN":
        if len(components['connections'].lines) > 0:
            curves.pop(components['connections'].selected)
    if id == "BUILDINSTALL":
        external = 1
        filename = prompt_file()
        print(filename)
        external = 0
    if id == "SAVE":
        objects = []
        objects.append(nodes)
        objects.append(curves)
        save_objects("save.pck",objects)

buttons = {}
buttons['add_comp'] = Button(pygame.Rect(0, 949, 200, 50), "Add comp", name="ADDCOMP")
buttons['add_comp'].callback = button_callback
buttons['del_last'] = Button(pygame.Rect(205, 949, 200, 50), "Del comp", name="DELLAST")
buttons['del_last'].callback = button_callback
buttons['build_install'] = Button(pygame.Rect(410, 949, 200, 50), "Build+Install", name="BUILDINSTALL")
buttons['build_install'].callback = button_callback
buttons['del_conn'] = Button(pygame.Rect(615, 949, 200, 50), "Del conn", name="DELCONN")
buttons['del_conn'].callback = button_callback
buttons['save'] = Button(pygame.Rect(820, 949, 200, 50), "Save work", name="SAVE")
buttons['save'].callback = button_callback

forms = Form(pygame.Rect(250,250,200,200),"test form", FONT, ["yellow","darkgray"], "black")

objects = load_objects("save.pck")
if objects:
    for obj in objects[0]:
        nodes.append(obj)
    for obj in objects[1]:
        curves.append(obj)

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
            for comp in components.keys():
                components[comp].handle_event(event)
            for button in buttons.keys():
                buttons[button].handle_event(event)
            #forms.handle_events(event=event)

    for node in nodes:
        node.draw(screen)

    for button in buttons.keys():
        buttons[button].draw(screen)

    k = []
    pinek = []
    for i, curve in enumerate(curves):
        t = 2
        if components['connections'].selected == i:
            t = 5
        if curve.Pin1 == None:
            k.append(BezierCurve(curve.Pin0.curveStart, pygame.mouse.get_pos(), offset=20, thickness=t, color=(255, 255, 0)))
            k[-1].color = color_pairs[i]
        else:
            k.append(BezierCurve(curve.Pin0.curveStart, curve.Pin1.curveStart, offset=20, thickness=t, color=(255, 255, 0)))
            slave = f"{curve.Pin0.parent.name}-{curve.Pin0.name}"
            master = f"{curve.Pin1.name}"
            if curve.Pin0.parent.master:
                master = f"{curve.Pin0.name}"
                slave = f"{curve.Pin1.parent.name}-{curve.Pin1.name}"
            pinek.append(f"{master} -> {slave}")
            k[-1].color = color_pairs[i]
        
        k[-1].set_dynamic_offset()
        k[-1].draw(screen)

    components["connections"].lines = pinek
    components["connections"].update()

    for comp in components.keys():
        components[comp].draw(screen)

    pygame.display.flip()
    clock.tick(60)

pygame.quit()