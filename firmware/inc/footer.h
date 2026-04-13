#ifndef FOOTER_H
#define FOOTER_H    
    // **********************************************************************************
    // ** the following code cunfigures the rest of the module please do not change it **
    // ** if you not know exactly what you are doing, it can break the module          **
    // **********************************************************************************

    #if raspberry_pi_spi == 1
        #pragma message("Build for Raspberry PI SPI communication!")
    #endif

    #if breakout_board == 1
        #pragma message "Configured to use BreakoutBoard"
        // if you use the breakout board, you need to change the pin configuration (4 stepgen, 2 encoder, 4 fast input, 1 pwm output,8 output, 16 inputs)
        #define MCP23017_ADDR   0x20
        #define MCP23008_ADDR   0x21
        #define MCP_ALL_RESET   21
        
        #undef encoders
        #undef enc_pins
        #undef enc_index_pins
        #undef enc_index_active_level
        #undef stepgens
        #undef stepgen_steps
        #undef stepgen_dirs
        #undef step_invert

        // all pin alias is defined in the internals.h if you want to use instead of using GPIO numbers
        // All GPIO form 0-15 and 22-31 are usable
        #define stepgens 4
    
        // defined with PINS
        #define stepgen_steps {PIN_1, PIN_4, PIN_6, PIN_9}
        #define stepgen_dirs {PIN_2, PIN_5, PIN_7, PIN_10}
        #define step_invert {0, 0, 0, 0, 0} // step pin invert for each stepgen (0 = not inverted, 1 = inverted)

        #define encoders 2
        #define enc_pins {8, 14} // uses 2 pins, you need to set the first pin (PIN_11 + PIN_12)
        #define enc_index_pins {10, 11}  // pin the encoder index is connected (interrupt driven)
        #define enc_index_active_level {high, high}

        #undef in_pins          
        #undef in_pullup
        #undef out_pins
        #undef use_pwm
        #define in_pullup       {0, 0, 0, 0}
        #define I2C_SDA         26
        #define I2C_SCK         27
        #define I2C_PORT        i2c1
        #define ANALOG_CH       2
        #define MCP4725_BASE    0x60   // range of 0x60 - 0x67
        #define MCP4725_PORT    i2c0
        #define MCP4725_SDA     12
        #define MCP4725_SCL     13
        #define pwm_count 0
    #endif // breakout_board > 0

    #if breakout_board == 2
        #pragma message "Configured to use IO BreakoutBoard"
        // if you use the IO breakout board (96 input, 32 output using 8 MCP23017)
        #define MCP23017_ADDR   0x20
        #define IN_EXPANDER_COUNT 6 // how many input expanders you connected to the breakout board (16 Input each max 6)
        #define MCP23017_ADDR_output   0x26
        #define output_expander_count 2 // how many output expanders you connected to the breakout board (16 output each max 2)
        #define MCP_ALL_RESET   GPIO_RESET
        
        #undef encoders
        #undef enc_pins
        #undef enc_index_pins
        #undef enc_index_active_level
        #undef stepgens
        #undef stepgen_steps
        #undef stepgen_dirs
        #undef step_invert
        #undef MCP4725_BASE

        #define toolchanger_encoder 1  // use of toolchanger encoder module, 4bit input with strobe and parity

        // all pin alias is defined in the internals.h if you want to use instead of using GPIO numbers
        // All GPIO form 0-15 and 22-31 are usable
        #define stepgens 0
    
        // defined with PINS
        #define stepgen_steps {}
        #define stepgen_dirs {}
        #define step_invert {} // step pin invert for each stepgen (0 = not inverted, 1 = inverted)

        #define encoders 0
        #define enc_pins {} // uses 2 pins, you need to set the first pin (PIN_11 + PIN_12)
        #define enc_index_pins {}  // pin the encoder index is connected (interrupt driven)
        #define enc_index_active_level {}

        #undef in_pins          
        #undef in_pullup
        #undef out_pins
        #undef use_pwm
        #define pwm_count 0
        #define in_pullup       {0}
        #define I2C_SDA         12
        #define I2C_SCK         13
        #define I2C_PORT        i2c0
    #endif // brakeout_board > 0

    #if breakout_board == 3
        #pragma message "Configured to use Analog BreakoutBoard"
        // if you use the analog breakout board (6x bipolar analog output using 6 MCP4725 and 6x high speed encoder counter) !!!! not implemented yet
        #define DA_CHANNELS    4
        #define ANALOG_CH       DA_CHANNELS
        #define MCP4725_BASE   0x60
        #define MCP4725_PORT   i2c1
        #define MCP4725_SDA    PIN_14
        #define MCP4725_SCL    PIN_15

        #define I2C_SDA         26
        #define I2C_SCK         27
        #define I2C_PORT        i2c1
        #define MCP23008_ADDR   0x20
        #define MCP_ALL_RESET   22

        #undef encoders
        #undef enc_pins
        #undef enc_index_pins
        #undef enc_index_active_level
        #undef stepgens
        #undef stepgen_steps
        #undef stepgen_dirs
        #undef step_invert

        // all pin alias is defined in the internals.h if you want to use instead of using GPIO numbers
        // All GPIO form 0-15 and 22-31 are usable
        #define stepgens 0
    
        // defined with PINS
        #define stepgen_steps {}
        #define stepgen_dirs {}
        #define step_invert {} // step pin invert for each stepgen (0 = not inverted, 1 = inverted)

        #define encoders 4
        #define enc_pins {PIN_1, PIN_5, PIN_9, PIN_16} // uses 2 pins, you need to set the first pin (PIN_11 + PIN_12)
        #define enc_index_pins {PIN_4, PIN_7, PIN_11, PIN_19}  // pin the encoder index is connected (interrupt driven)
        #define enc_index_active_level {high, high, high, high}

        #undef in_pins          
        #undef in_pullup
        #undef out_pins
        #undef use_pwm
        #define pwm_count 0

    #endif // brakeout_board > 0

    #if breakout_board == 100
        #pragma message "Configured to use BreakoutBoard100"
        #undef raspberry_pi_spi
        #define raspberry_pi_spi 1
        // breakoutboard_100: 4 stepgen, 2 encoder, 32 input, 16 output via MCP23017
        #define MCP23017_ADDR          0x20
        #define IN_EXPANDER_COUNT      2   // 2x16 digital inputs
        #define MCP23017_ADDR_output   0x22
        #define output_expander_count  1   // 1x16 digital outputs
        #define MCP_ALL_RESET          GPIO_RESET
        #define ANALOG_CH               2
        #define MCP4725_BASE            0x60   // range of 0x60 - 0x67
        #define MCP4725_PORT            i2c0
        #define MCP4725_SDA             12
        #define MCP4725_SCL             13

        #undef encoders
        #undef enc_pins
        #undef enc_index_pins
        #undef enc_index_active_level
        #undef stepgens
        #undef stepgen_steps
        #undef stepgen_dirs
        #undef step_invert

        #define stepgens 4
        #define stepgen_steps {PIN_1, PIN_4, PIN_6, PIN_9}
        #define stepgen_dirs {PIN_2, PIN_5, PIN_7, PIN_10}
        #define step_invert {0, 0, 0, 0, 0}

        #define encoders 2
        #define enc_pins {8, 14}
        #define enc_index_pins {10, 11}
        #define enc_index_active_level {high, high}

        #undef in_pins
        #undef in_pullup
        #undef out_pins
        #undef use_pwm
        #define use_pwm             0
        #define pwm_count           0
        #define in_pullup           {0, 0, 0, 0}

        #define default_pulse_width 2000
        #define default_step_scale  1000

        #define I2C_SDA         26
        #define I2C_SCK         27
        #define I2C_PORT        i2c1
    #endif // breakout_board == 100

    #define use_stepcounter 0 // Use step counter for the stepgen
    #define debug_mode 0   // only used in Raspberry PI communications
    #define max_statemachines stepgens + encoders
    
    #ifdef PICO_RP2040
        #if max_statemachines > 8
            #pragma error "State machines exceeded the maximum platform size (8)."
        #endif
    #endif
    #ifdef PICO_RP2350
        #if max_statemachines > 12
            #pragma error "State machines exceeded the maximum platform size (12)."
        #endif
    #endif

    #define pico_clock 200000000

#endif