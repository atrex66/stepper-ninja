# -- coding: utf-8 --

import socket
import struct
from jump_table import jump_table

tx_size = 25
rx_size = 25
high_cycles = 198

class StepperNinja:
    def __init__(self, pico_ip="192.168.0.177", port=8888, timeout=1.0):
        """UDP kommunikáció inicializálása."""
        self.pico_ip = pico_ip
        self.port = port
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.sock.bind(("0.0.0.0", port))
        self.sock.settimeout(timeout)
        print(f"UDP socket created on {self.pico_ip}:{self.port}")
        self.jump_index_out = 1
        self.jump_index_inp = 1
        self.outputs = 0  # Kezdeti kimenetek (output-00 - output-07)
        self.inputs = 0   # Kezdeti bemenetek (input-00 - input-07)
        self.analog_scale = 4095
        self.tx_buffer = bytearray(tx_size)
        self.rx_buffer = bytearray(rx_size)
        self.timing = []
        self.state = {
        'sent_count': 0,
        'last_inputs': 0,
        'last_addr': "N/A",
        'last_error': ""
        }
        self.generate_timing()  # Inicializáláskor generáljuk a timing táblát

    def generate_timing(self, period = 1000000, high_cycles = 198):
        # total_cycles kiszámítása: period (µs) * 125 MHz órajel / 1e6
        total_cycles = 125000  # Egész osztás, uint32_t
        total_cycles &= 0xFFFFFFFF  # 32 bites határ biztosítása
        print(f"total_cycles = {total_cycles} (period = {period} µs)")
        # Timing tömb inicializálása (256 elem, 0-tól 255-ig)
        self.timing = [0] * 256

        # Pregenerálás i=1-től 255-ig
        for i in range(1, 256):
            step_counter = (total_cycles // i) - high_cycles  # Egész osztás
            step_counter &= 0xFFFFFFFF  # 32 bites határ
            pio_cmd = (step_counter << 8) | (i - 1)  # Balra tolás és index-1
            pio_cmd &= 0xFFFFFFFF  # 32 bites határ
            self.timing[i] = pio_cmd
        return self.timing
       

    def copy_timing_to_tx_buffer(self, index, buffer_index=0):
        """
        Egy timing[i] érték másolása a tx_buffer-be.
        Args:
            timing: Lista vagy tömb 256 uint32_t elemmel.
            index: A másolandó index (0-255).
            tx_buffer: Cél bytearray, ha None, újat hoz létre.
        Returns:
            bytearray: A kitöltött tx_buffer.
        """
        if index > 255:
            raise ValueError("Index must be 0-255")
        
        # uint32_t little-endian bájtokra bontása
        pio_cmd = self.timing[index]
        struct.pack_into("<I", self.tx_buffer, buffer_index  * 4, pio_cmd)
    

    def jump_in_table_out(self, input):
        """
        Ugrás a táblában két bemenet bináris összege + 1 alapján (8 bites túlcsordulással).
        input1, input2: 0-255 közötti egész számok (bájtok)
        Visszatér: A tábla ((input1 + input2) % 256 + 1) % 256 indexén található érték
        """

        v = 0
        for i in range(tx_size - 1):
            v += input[i]
        index = (v) % 256

        # +1 hozzáadása az indexhez, szintén 8 bites határon belül
        self.jump_index_out += index
        self.jump_index_out %= 256  # Biztosítjuk, hogy az index 0-255 között maradjon
        
        # Táblában való ugrás
        result = jump_table[self.jump_index_out]
        return result

    def jump_in_table_inp(self, input):
        """
        Ugrás a táblában két bemenet bináris összege + 1 alapján (8 bites túlcsordulással).
        input1, input2: 0-255 közötti egész számok (bájtok)
        Visszatér: A tábla ((input1 + input2) % 256 + 1) % 256 indexén található érték
        """
       
        # 8 bites bináris összeadás túlcsordulással: (input1 + input2) % 256
        v = 0
        for i in range(rx_size - 1):
            v += input[i]
        index = (v) % 256

        # +1 hozzáadása az indexhez, szintén 8 bites határon belül
        self.jump_index_inp += index
        self.jump_index_inp %= 256  # Biztosítjuk, hogy az index 0-255 között maradjon

        # Táblában való ugrás
        result = jump_table[self.jump_index_inp]
        return result

    def print_hex(self, data):
        """Hexadecimális értékek kiírása."""
        hex_string = " ".join(f"{byte:02X}" for byte in data)
        print(hex_string)


    def send_request(self):
        """Kimenetek elküldése a Pico-nak."""
        self.tx_buffer[tx_size - 1] = self.jump_in_table_out(self.tx_buffer)
        self.print_hex(self.tx_buffer)
        self.sock.sendto(self.tx_buffer, (self.pico_ip, self.port))
        return 0

    def receive_inputs(self):
        """Bemenetek fogadása a Pico-tól."""
        try:
            data, addr = self.sock.recvfrom(rx_size)
            if len(data) == rx_size:
                calc_checksum = self.jump_in_table_inp(data)
                if calc_checksum == data[rx_size - 1]:
                    self.rx_buffer = data
                    return 0, 0
                else:
                    print(f"addr: {addr[0]}")
                    print(f"Checksum error! Expected {calc_checksum:02X}, got {data[4]:02X}")
                    exit()
                    return None, f"Checksum error! Expected {calc_checksum:02X}, got {data[4]:02X}"
            else:
                return None, f"Invalid packet size: {len(data)} bytes"
        except socket.timeout:
            return None, "No response from Pico"
        except OSError as e:
            return None, f"Socket error: {e}"

    # Set Output state
    # 0-7: output-00 - output-07
    # 8: output-08 (low pass filter) enable/disable
    def set_ouput(self, out, state):
        """Kimenet beállítása."""
        if state:
            self.outputs |= (1 << out)
        else:
            self.outputs &= ~(1 << out)

    def get_output(self, output):
        """Kimenet lekérdezése."""
        if self.outputs & (1 << output):
            return True
        else:
            return False

    def get_input(self, input):
        """Bemenet lekérdezése."""
        if self.inputs & (1 << input):
            return True
        else:
            return False

    def get_analog_input(self):
        lower_byte = self.inputs>> 16 & 0xFF
        upper_byte = (self.inputs >> 24) & 0x1F
        # Az alsó és felső bájtok összevonása
        adc_value = (upper_byte << 8) | lower_byte
        # Az ADC érték skálázása a self.analog_scale alapján
        scaled_value = adc_value * (self.analog_scale / 4095)
        return scaled_value

    def enable_low_pass_filter(self):
        self.set_ouput(8, True)  # Enable low pass filter
    
    def disable_low_pass_filter(self):
        self.set_ouput(8, False)

    def update(self):
         # Network communication
        try:
            sent_msg = self.send_request()
            self.state['sent_count'] += 1
        except Exception as e:
            self.state['last_error'] = f"Send error: {str(e)}"
            print(f"Send error: {self.state['last_error']}")

        try:
            inputs, addr = self.receive_inputs()
            if inputs is not None:
                self.state['last_inputs'] = inputs
                self.state['last_addr'] = addr
                self.state['last_error'] = ""
        except Exception as e:
            self.state['last_error'] = str(e)
            print(f"Receive error: {self.state['last_error']}")

    def close(self):
        """Socket lezárása."""
        self.sock.close()
