from PIL import Image
import sys
import argparse
import math

def rgb_to_rgb565(r, g, b):
    """RGB888-ból RGB565-be konvertálás."""
    r = (r >> 3) & 0x1F  # 5 bit vörös
    g = (g >> 2) & 0x3F  # 6 bit zöld
    b = (b >> 3) & 0x1F  # 5 bit kék
    return (r << 11) | (g << 5) | b

def png_to_header(input_png, output_h, glyph_width, glyph_height, array_name="bitmap_font", color_depth="monochrome"):
    # PNG betöltése
    img = Image.open(input_png)
    width, height = img.size
    if width % glyph_width != 0 or height % glyph_height != 0:
        raise ValueError(f"A kép mérete ({width}x{height}) nem osztható a glif mérettel ({glyph_width}x{glyph_height})!")

    # Glifek száma
    chars_x = width // glyph_width
    chars_y = height // glyph_height
    total_chars = chars_x * chars_y

    # Header fájl tartalma
    header = [
        "#ifndef BITMAP_FONT_H",
        "#define BITMAP_FONT_H",
        "#include <stdint.h>",
        "",
        f"#define GLYPH_WIDTH {glyph_width}",
        f"#define GLYPH_HEIGHT {glyph_height}",
        f"#define TOTAL_GLYPHS {total_chars}",
    ]

    if color_depth == "monochrome":
        bytes_per_row = math.ceil(glyph_width / 8)
        header.append(f"#define BYTES_PER_ROW {bytes_per_row}")
        header.append(f"static const uint8_t {array_name}[{total_chars}][GLYPH_HEIGHT][BYTES_PER_ROW] = {{")
        img = img.convert("L")  # Szürkeárnyalatos mód monokrómhoz
    else:  # rgb565
        header.append(f"static const uint16_t {array_name}[{total_chars}][GLYPH_HEIGHT][GLYPH_WIDTH] = {{")
        img = img.convert("RGB")  # RGB mód színeshez

    # Glifek feldolgozása
    for cy in range(chars_y):
        for cx in range(chars_x):
            glyph_data = []
            for y in range(glyph_height):
                row_data = []
                if color_depth == "monochrome":
                    for byte_idx in range(bytes_per_row):
                        byte = 0
                        for x in range(8):
                            pixel_x = cx * glyph_width + byte_idx * 8 + x
                            if pixel_x < cx * glyph_width + glyph_width:
                                pixel = img.getpixel((pixel_x, cy * glyph_height + y))
                                bit = 1 if pixel > 128 else 0
                                byte |= (bit << (7 - x))
                        row_data.append(f"0x{byte:02X}")
                    glyph_data.append(f"{{ {', '.join(row_data)} }}")
                else:  # rgb565
                    for x in range(glyph_width):
                        r, g, b = img.getpixel((cx * glyph_width + x, cy * glyph_height + y))
                        rgb565 = rgb_to_rgb565(r, g, b)
                        row_data.append(f"0x{rgb565:04X}")
                    glyph_data.append(f"{{ {', '.join(row_data)} }}")
            header.append(f"    {{ {', '.join(glyph_data)} }}, // Char {(cy * chars_x + cx)}")
    
    header.extend([
        "};",
        "",
        "#endif // BITMAP_FONT_H"
    ])

    # Fájl írása
    with open(output_h, "w") as f:
        f.write("\n".join(header))
    print(f"Header fájl generálva: {output_h}")

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="PNG-ből C header fájl generálása bitmapfontokhoz.")
    parser.add_argument("input_png", help="Bemeneti PNG fájl")
    parser.add_argument("output_h", help="Kimeneti header fájl")
    parser.add_argument("-w", "--width", type=int, default=8, help="Glif szélessége (pixel)")
    parser.add_argument("-h", "--height", type=int, default=8, help="Glif magassága (pixel)")
    parser.add_argument("-n", "--array-name", default="bitmap_font", help="Tömb neve a headerben")
    parser.add_argument("-c", "--color-depth", choices=["monochrome", "rgb565"], default="monochrome", help="Színmélység: monochrome vagy rgb565")
    args = parser.parse_args()

    try:
        png_to_header(args.input_png, args.output_h, args.width, args.height, args.array_name, args.color_depth)
    except Exception as e:
        print(f"Hiba: {e}")
        sys.exit(1)