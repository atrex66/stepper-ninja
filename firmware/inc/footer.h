    
    // **********************************************************************************
    // ** the following code cunfigures the rest of the module please do not change it **
    // ** if you not know exactly what you are doing, it can break the module          **
    // **********************************************************************************

    #if raspberry_pi_spi == 1
        #pragma message("Build for Raspberry PI SPI communication!")
    #endif

    #if brakeout_board > 0
        // if you use the breakout board, you need to change the pin configuration (4 stepgen, 2 encoder, 4 fast input, 1 pwm output,8 output, 16 inputs)
        #define MCP23017_ADDR   0x21
        #define MCP23008_ADDR   0x20
        #define MCP_ALL_RESET   22
        #define encoders        2
        #define in_pins         {12, 13, 15, 28} // Free GPIO pins for inputs (GPIO 22-28)
        #define in_pullup       {0, 0, 0, 0}
        #define I2C_SDA         26
        #define I2C_SCK         27
    #endif // brakeout_board > 0

    #define use_stepcounter 0 // Use step counter for the stepgen
    #define use_timer_interrupt 0 // Use timer interrupt for the stepgen starting, maybe eliminates servo-thread jitter experimental
    #define debug_mode 0    // only used in Raspberry PI communications
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
