# Konstansok magyarázattal
PICO_RUN_TIME = 125000
# PICO_RUN_TIME: Az órajelciklusok száma egy szervo-periódus alatt, 125 MHz-en.
# 1 ciklus = 8 ns, így 125000 * 8 ns = 10^6 ns = 1 ms.

SERVO_PERIOD_NS = PICO_RUN_TIME * 8
# SERVO_PERIOD_NS: A szervo-periódus hossza nanoszekundumban.
# 125000 ciklus * 8 ns/ciklus = 1_000_000 ns = 1 ms.

PIO_CYCLE_TIME_NS = 16
# PIO_CYCLE_TIME_NS: A PIO program ciklusideje nanoszekundumban.
# A PIO program 2 órajelciklus alatt fut (2 * 8 ns = 16 ns/bit).

def calculate_edge_times(prev_steps, new_steps):
    """
    Felfutó élek időpontjainak kiszámítása, hogy N_new lépés beleférjen az 1 ms-be,
    pontos utolsó periódussal.
    Bemenetek:
        prev_steps: előző lépésszám (N_prev)
        new_steps: új lépésszám (N_new)
    Kimenet:
        edge_times: felfutó élek időpontjai ns-ban
        bit_positions: bitpozíciók a PIO-hoz (16 ns/bit)
    """
    if new_steps <= 1:
        return [], []

    # Kezdő és végső periódus
    start_period_ns = SERVO_PERIOD_NS / prev_steps  # T_0 = 10^6 / N_prev
    end_period_ns = SERVO_PERIOD_NS / new_steps     # T_(N-1) = 10^6 / N_new

    # Periódus lépésenkénti változása
    delta_period_ns = (end_period_ns - start_period_ns) / (new_steps - 1)

    # Időpontok kiszámítása
    edge_times = []
    total_time_ns = 0
    for i in range(new_steps - 1):
        edge_times.append(total_time_ns)
        period_ns = start_period_ns + i * delta_period_ns
        total_time_ns += period_ns

    # Az utolsó időpont úgy, hogy T_(N-1) = end_period_ns
    edge_times.append(total_time_ns)
    total_time_ns += end_period_ns
    edge_times.append(total_time_ns)

    # Az utolsó időpont nem lehet nagyobb, mint 10^6 ns
    if edge_times[-1] > SERVO_PERIOD_NS:
        print(f"Figyelmeztetés: Az utolsó időpont túllépi az 1 ms-t ({edge_times[-1]:.1f} ns > {SERVO_PERIOD_NS} ns)")
        # Skálázás az 1 ms-be illesztéshez
        scale = SERVO_PERIOD_NS / edge_times[-1]
        edge_times = [t * scale for t in edge_times]

    # Bitpozíciók
    bit_positions = [int(t // PIO_CYCLE_TIME_NS) for t in edge_times]

    return edge_times, bit_positions

# Tesztprogram
def main():
    # Példa bemenet
    prev_steps = 10  # Előző lépésszám
    new_steps = 20   # Új lépésszám

    # Felfutó élek kiszámítása
    edge_times, bit_positions = calculate_edge_times(prev_steps, new_steps)

    # Eredmények kiírása
    print("Dinamic sub servo-thread acceleration calculation")
    print(f"previous steps/servo-thread: {prev_steps}")
    print(f"new step #: {new_steps}")
    print(f"servo-period: {SERVO_PERIOD_NS} ns (1 ms)")
    print(f"pio cycle time: {PIO_CYCLE_TIME_NS} ns/bit")
    print("\nrising edges position(ns), bit positions and periods:")
    for i, (time_ns, bit_pos) in enumerate(zip(edge_times, bit_positions)):
        period_ns = edge_times[i + 1] - time_ns if i < len(edge_times) - 1 else None
        period_str = f", period: {period_ns:.1f} ns" if period_ns is not None else ""
        print(f"t_{i} = {time_ns:.1f} ns, step pulse {i} start at = {bit_pos} bit{period_str}")

    # Lépésimpulzusok távolsága bitekben
    if len(bit_positions) > 1:
        print("\nrising edges distance in bits:")
        for i in range(len(bit_positions) - 1):
            step_distance_bits = bit_positions[i + 1] - bit_positions[i]
            last_accel = 0
            if i > 0:
                last_accel = step_distance_bits - (bit_positions[i] - bit_positions[i - 1])
            print(f"step pulse {i} and {i + 1} distance = {step_distance_bits} bit change in bit pos:{last_accel}" )

    # Ellenőrzés: utolsó lépés periódusa
    if len(edge_times) > 1:
        last_period_ns = edge_times[-1] - edge_times[-2]
        print(f"\nLast period time: {last_period_ns:.1f} ns "
              f"(f_end = {1e9 / last_period_ns:.2f} Hz)")

if __name__ == "__main__":
    main()