#!/usr/bin/env python3
import argparse
import os
import subprocess
import sys
import time

import hal

try:
    import pygame
except ImportError as exc:
    print("pygame is required. Install with: pip install pygame", file=sys.stderr)
    raise SystemExit(1) from exc


def clamp(value, lo, hi):
    return max(lo, min(hi, value))


def load_postgui_hal(halfile):
    cmd = ["halcmd", "-f", halfile]
    result = subprocess.run(cmd, check=False, capture_output=True, text=True)
    if result.returncode != 0:
        print("Failed to load postgui HAL file:", halfile, file=sys.stderr)
        if result.stdout:
            print(result.stdout, file=sys.stderr)
        if result.stderr:
            print(result.stderr, file=sys.stderr)
        raise SystemExit(result.returncode)


def create_component():
    comp = hal.component("bb1-ui")

    comp.newpin("connected", hal.HAL_BIT, hal.HAL_IN)
    comp.newpin("io_ready", hal.HAL_BIT, hal.HAL_IN)

    for i in range(16):
        comp.newpin(f"in{i:02d}", hal.HAL_BIT, hal.HAL_IN)

    for i in range(8):
        comp.newpin(f"out{i:02d}", hal.HAL_BIT, hal.HAL_OUT)

    comp.newpin("analog0", hal.HAL_FLOAT, hal.HAL_OUT)
    comp.newpin("analog1", hal.HAL_FLOAT, hal.HAL_OUT)

    # Encoder 0 monitor/control pins
    comp.newpin("enc0_raw_count", hal.HAL_S32, hal.HAL_IN)
    comp.newpin("enc0_position", hal.HAL_FLOAT, hal.HAL_IN)
    comp.newpin("enc0_velocity_rps", hal.HAL_FLOAT, hal.HAL_IN)
    comp.newpin("enc0_velocity_rpm", hal.HAL_FLOAT, hal.HAL_IN)
    comp.newpin("enc0_index_enable", hal.HAL_BIT, hal.HAL_OUT)

    comp.newpin("heartbeat", hal.HAL_U32, hal.HAL_OUT)

    for i in range(8):
        comp[f"out{i:02d}"] = 0
    comp["analog0"] = 0.0
    comp["analog1"] = 0.0
    comp["enc0_index_enable"] = 0
    comp["heartbeat"] = 0

    comp.ready()
    return comp


def draw_led(screen, x, y, on, label, font):
    color = (40, 200, 80) if on else (80, 80, 80)
    pygame.draw.circle(screen, color, (x, y), 14)
    txt = font.render(label, True, (220, 220, 220))
    screen.blit(txt, (x - txt.get_width() // 2, y + 20))


def draw_button(screen, rect, on, label, font):
    bg = (30, 150, 230) if on else (55, 55, 55)
    pygame.draw.rect(screen, bg, rect, border_radius=8)
    pygame.draw.rect(screen, (200, 200, 200), rect, width=2, border_radius=8)
    txt = font.render(label, True, (240, 240, 240))
    screen.blit(txt, (rect.centerx - txt.get_width() // 2, rect.centery - txt.get_height() // 2))


def main():
    parser = argparse.ArgumentParser(description="BreakoutBoard 1 HAL pygame test panel")
    parser.add_argument(
        "--postgui",
        default=os.path.join(os.path.dirname(__file__), "bb1_postgui.hal"),
        help="Path to postgui HAL file (default: %(default)s)",
    )
    parser.add_argument(
        "--skip-load-postgui",
        action="store_true",
        help="Do not load postgui HAL file (useful if already loaded)",
    )
    args = parser.parse_args()

    comp = create_component()

    if not args.skip_load_postgui:
        load_postgui_hal(args.postgui)

    pygame.init()
    screen = pygame.display.set_mode((980, 560))
    pygame.display.set_caption("Stepper-Ninja BreakoutBoard1 Test Panel")
    clock = pygame.time.Clock()
    font = pygame.font.SysFont("DejaVu Sans", 20)
    small = pygame.font.SysFont("DejaVu Sans", 16)
    big = pygame.font.SysFont("DejaVu Sans", 42)

    out_state = [False] * 8
    analog0 = 0.0
    analog1 = 0.0
    enc0_index_enable = False
    running = True
    last_hb = time.monotonic()

    while running:
        for event in pygame.event.get():
            if event.type == pygame.QUIT:
                running = False
            elif event.type == pygame.KEYDOWN:
                if event.key == pygame.K_ESCAPE:
                    running = False
                elif pygame.K_1 <= event.key <= pygame.K_8:
                    idx = event.key - pygame.K_1
                    out_state[idx] = not out_state[idx]
                elif event.key == pygame.K_q:
                    analog0 = clamp(analog0 + 0.1, 0.0, 10.0)
                elif event.key == pygame.K_a:
                    analog0 = clamp(analog0 - 0.1, 0.0, 10.0)
                elif event.key == pygame.K_w:
                    analog1 = clamp(analog1 + 0.1, 0.0, 10.0)
                elif event.key == pygame.K_s:
                    analog1 = clamp(analog1 - 0.1, 0.0, 10.0)
                elif event.key == pygame.K_i:
                    enc0_index_enable = not enc0_index_enable

        for i in range(8):
            comp[f"out{i:02d}"] = int(out_state[i])
        comp["analog0"] = analog0
        comp["analog1"] = analog1
        comp["enc0_index_enable"] = int(enc0_index_enable)

        now = time.monotonic()
        if now - last_hb > 0.1:
            comp["heartbeat"] = (int(comp["heartbeat"]) + 1) & 0xFFFFFFFF
            last_hb = now

        screen.fill((22, 25, 30))

        title = font.render("BreakoutBoard 1 Test Panel", True, (240, 240, 240))
        screen.blit(title, (20, 16))

        enc_pos_big = big.render(
            f"ENC0 POS {float(comp['enc0_position']):.4f}",
            True,
            (255, 220, 120),
        )
        screen.blit(enc_pos_big, (520, 18))

        status = small.render(
            f"connected={int(comp['connected'])}  io_ready={int(comp['io_ready'])}  "
            f"analog0={analog0:.2f}  analog1={analog1:.2f}",
            True,
            (200, 210, 220),
        )
        screen.blit(status, (20, 48))

        enc_status = small.render(
            f"enc0_raw={int(comp['enc0_raw_count'])}  "
            f"enc0_pos={float(comp['enc0_position']):.4f}  "
            f"enc0_rps={float(comp['enc0_velocity_rps']):.4f}  "
            f"enc0_rpm={float(comp['enc0_velocity_rpm']):.2f}  "
            f"index_en={int(enc0_index_enable)}",
            True,
            (200, 210, 220),
        )
        screen.blit(enc_status, (20, 68))

        hint = small.render(
            "Keys: 1..8 outputs, Q/A analog0, W/S analog1, I index-enable, Esc quit",
            True,
            (180, 190, 205),
        )
        screen.blit(hint, (20, 92))

        for i in range(16):
            col = i % 8
            row = i // 8
            x = 80 + col * 110
            y = 170 + row * 90
            draw_led(screen, x, y, bool(comp[f"in{i:02d}"]), f"IN{i:02d}", small)

        out_rects = []
        for i in range(8):
            rect = pygame.Rect(60 + i * 110, 380, 90, 48)
            out_rects.append(rect)
            draw_button(screen, rect, out_state[i], f"OUT{i:02d}", small)

        if pygame.mouse.get_pressed()[0]:
            mx, my = pygame.mouse.get_pos()
            for i, rect in enumerate(out_rects):
                if rect.collidepoint(mx, my):
                    out_state[i] = not out_state[i]
                    time.sleep(0.12)
                    break

        pygame.display.flip()
        clock.tick(30)

    pygame.quit()


if __name__ == "__main__":
    try:
        main()
    except KeyboardInterrupt:
        pass
