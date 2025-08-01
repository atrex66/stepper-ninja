#ifndef FOOTER_H
#define FOOTER_H    
    // **********************************************************************************
    // ** the following code cunfigures the rest of the module please do not change it **
    // ** if you not know exactly what you are doing, it can break the module          **
    // **********************************************************************************

    #if raspberry_pi_spi == 1
        #pragma message("Build for Raspberry PI SPI communication!")
    #endif

    #if breakout_board > 0
        #pragma message "Configured to use BreakoutBoard"
        // if you use the breakout board, you need to change the pin configuration (4 stepgen, 2 encoder, 4 fast input, 1 pwm output,8 output, 16 inputs)
        #define MCP23017_ADDR   0x20
        #define MCP23008_ADDR   0x21
        #define MCP_ALL_RESET   GPIO_RESET
        #undef encoders
        #define encoders        2
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
    #endif // brakeout_board > 0

    #define use_stepcounter 0 // Use step counter for the stepgen
    #define use_timer_interrupt 0 // Use timer interrupt for the stepgen starting, maybe eliminates servo-thread jitter experimental
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