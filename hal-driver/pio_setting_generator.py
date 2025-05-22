# Python script a C header fájl generálására rendezett lookup táblával, duplikátumok nélkül
import datetime

def generate_lookup_header():
    # Header fájl neve
    output_file = "pio_settings.h"

    # Tartományok és eltolások
    x_min, x_max = 4, 12
    y_min, y_max = 16, 32

    # Lookup tábla adatainak kiszámítása
    settings = []
    for x in range(x_min, x_max):
        for y in range(y_min, y_max):
            # Korrigált indexszámítás (csak nyomon követéshez)
            calc_z = ((x + 2.0) * (y + 1)) * 8
            print(f"Calculating for x: {x}, y: {y} => calc_z: {calc_z}")
            settings.append((y, x, round(calc_z / 8)))

    # Rendezés high_cycles szerint (calc_z a 3-as indexű elem)
    settings.sort(key=lambda s: s[2])

    # Duplikátumok eltávolítása, csak az első előfordulást tartjuk meg
    unique_settings = []
    seen_high_cycles = set()
    for entry in settings:
        high_cycles = entry[2]
        if high_cycles not in seen_high_cycles:
            unique_settings.append(entry)
            seen_high_cycles.add(high_cycles)

    # A végső tömb mérete
    max_settings = len(unique_settings)

    # Header fájl tartalmának összeállítása
    header_content = f"""\
/* Automatically generated header file for pio_settings lookup table */
/* Sorted by high_cycles, duplicates removed (keeping first occurrence) */
/* Generated on: {datetime.datetime.now().strftime("%Y-%m-%d %H:%M:%S")} */

#ifndef PIO_SETTINGS_H
#define PIO_SETTINGS_H

typedef struct {{
    int sety;
    int nop;
    double high_cycles;
}} pio_setting_t;

static const pio_setting_t pio_settings[{max_settings}] = {{
"""

    # Értékek hozzáadása a tömbhöz, rendezett sorrendben, duplikátumok nélkül
    for i, (sety, nop, high_cycles) in enumerate(unique_settings):
        header_content += f"    {{ {sety}, {nop}, {high_cycles} }}{',' if i < max_settings - 1 else ''}\n"

    header_content += """\
};

#endif /* PIO_SETTINGS_H */
"""

    # Fájl írása
    try:
        with open(output_file, 'w') as f:
            f.write(header_content)
        print(f"Header fajl sikeresen generalva: {output_file}")
        print(f"Tomb merete: {max_settings} elem (duplikatumok eltavolitva)")
    except Exception as e:
        print(f"Hiba a fajl generalasa soran: {e}")

if __name__ == "__main__":
    generate_lookup_header()