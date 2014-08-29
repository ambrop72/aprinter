rec {
    melzi = {
        platform = "avr";
        targetVars = {
            F_CPU = "16000000";
            MCU = "atmega1284p";
        };
    };
    
    ramps13 = {
        platform = "avr";
        targetVars = {
            F_CPU = "16000000";
            MCU = "atmega2560";
        };
    };
    
    megatronics3 = {
        platform = "avr";
        targetVars = {
            F_CPU = "16000000";
            MCU = "atmega2560";
        };
    };
    
    arduino_due = {
        platform = "sam3x";
        targetVars = {
            ASF_BOARD = "43";
            ARCH = "sam3x";
            SUBARCH = "8";
            SUBSUBARCH = "e";
            USE_USB_SERIAL = "1";
        };
    };
    
    rampsfd = arduino_due;
    
    radds = arduino_due;
    
    "4pi" = {
        platform = "sam3x";
        targetVars = {
            ASF_BOARD = "33";
            ARCH = "sam3u";
            SUBARCH = "4";
            SUBSUBARCH = "e";
            USE_USB_SERIAL = "1";
        };
    };
    
    mlab = {
        platform = "sam3x";
        targetVars = {
            ASF_BOARD = "32";
            ARCH = "sam3s";
            SUBARCH = "2";
            SUBSUBARCH = "a";
            AT91SAM_ADC_TRIGGER_ERRATUM = "1";
            USE_USB_SERIAL = "1";
        };
    };
    
    teensy3 = {
        platform = "teensy";
        targetVars = {
            F_CPU = "96000000";
            TEENSY_VERSION = "3.1";
        };
    };
}
