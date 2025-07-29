import pygame
from gui import *
from predefined import *

screen = pygame.display.set_mode((1280, 1000))
pygame.display.set_caption("Stepper-Ninja Configurator !ALPHA!")
clock = pygame.time.Clock()

nodes = []

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


# Fő ciklus
running = True
while running:
    screen.fill(GRAY)
    
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

    # pico.draw(screen)

    for button in buttons:
        button.draw(screen)

    k = []
    pinek = []
    for i, curve in enumerate(curves):
        color = (255, 255, 0)
        if i == components[-1].selected:
            color = (0, 255, 255)
        if curve.Pin1 == None:
            k.append(BezierCurve(curve.Pin0.curveStart, pygame.mouse.get_pos(), offset=20, thickness=2, color=color))    
        else:
            k.append(BezierCurve(curve.Pin0.curveStart, curve.Pin1.curveStart, offset=20, thickness=2, color=color))
            pinek.append(f"{curve.Pin0.parent}-{curve.Pin0.name} -> {curve.Pin1.parent}-{curve.Pin1.name}")

        k[-1].set_dynamic_offset()
        k[-1].draw(screen)

    for comp in components:
        if comp.name == "connections":
            comp.lines = pinek
            comp.update()
        comp.draw(screen)

    pygame.display.flip()
    clock.tick(60)

pygame.quit()