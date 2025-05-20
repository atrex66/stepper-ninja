from stepper_ninja import StepperNinja

bit = 0
state = 0


# connect to the io-samurai
ninja_0 = StepperNinja(pico_ip="192.168.0.177", port=8888, timeout=1.0)

while True:
    # send and receive data from the io-samurai
    ninja_0.copy_timing_to_tx_buffer(32, 1)
    ninja_0.update()

    # try to exit programmed to prevent checksum errors (pico side, only reset clear the checksum error)
    # do not wait because triggering the pico timeout error (100mS)

samurai.close()