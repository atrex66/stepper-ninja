#ifndef SN_MACH3_3AXIS_H
#define SN_MACH3_3AXIS_H
/* GPIO numbers, independent of the Pico, Mach3 and LinuxCNC SDKs. */
#define SN_MACH3_PROFILE_ID 0x03070106u
#define SN_MACH3_AXES 3
#define SN_MACH3_INPUTS 7
#define SN_MACH3_OUTPUTS 6
#define SN_MACH3_PWM_CHANNELS 1
#define SN_MACH3_STEP_PINS {0, 2, 4}
#define SN_MACH3_DIR_PINS {1, 3, 5}
#define SN_MACH3_INPUT_PINS {13, 14, 15, 22, 26, 27, 28}
#define SN_MACH3_OUTPUT_PINS {7, 8, 9, 10, 11, 12}
#define SN_MACH3_PWM_PIN 6
#define SN_MACH3_INPUT_GPIO_MASK 0x1c40e000u
#endif
