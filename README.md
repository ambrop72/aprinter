aprinter
========

APrinter is a currently experimantal firmware for RepRap 3D printers and is under heavy development.

## Implemented features (possibly with bugs)

  * SD card printing (reading of sequential blocks only, no filesystem or partition support).
  * Serial communication using the defacto RepRap protocol. Baud rate above 57600 will not work, for now.
  * Homing, including homing of multiple axes at the same time. Either min- or max- endstops can be used.
  * Line motion with acceleration control and cartesian speed limit (F parameter).
    Speed limit in case of E-only motion is not implemented.
    The speed is automatically limited to not overload the MCU with interrupts.
  * Look-ahead to preserve some speed on corners. By default, planning takes the previous 3 moves into account.
    This can be increased in the configuration, but this
    also increases the chance of buffer underruns , which cause the print to temporarily pause while the buffer refills.
    Look-ahead is very memory-hungry in its current state.
  * Heater control using PID or on-off control. The thermistor tables need to be generated with a Python script.
  * Safe temperature range. A heater is turned off in case its temperature goes beyound the safe range.
  * Fan control.
  * Starting and feeding of the watchdog timer.
  * Emergency shutdown of motors and heaters in case of an assertion failure
    (if assertions are enabled with -DAMBROLIB_ASSERTIONS).
  * Non-drifting heartbeat LED. Its period is exactly 1 second, subject to the precision of your oscillator.

## Planned features (in the approximate order of priority):

  * Further optimization of the planning and stepping code, both for speed and memory usage.
  * Runtime configurability and settings in EEPROM.
  * SD card FAT32 support and write support.

## Hardware requirements

Ports have been completed for the following boards:

  * Melzi (atmega1284p only),
  * RAMPS 1.0, 1.1/1.2 or 1.3/1.4 (only RAMPS 1.4 with atmega2560 is tested).

However, any AVR satisfying the following should work, possibly requiring minor adjustments in the code:

  * 8kB of SRAM,
  * 128kB of flash,
  * 4 timers (any one of these can be either 8- or 16-bit).

## Coding style

  * Extreme attention to detail and bugless code. Lots of assertions (the proven kind, not guesses).
  * The software is written in C++11.
  * Extensive use of abstractions for hardware access and program structuring.
    It should be possible to port the software to non-AVR platforms with little work.
  * Template metaprogramming is used to implement the abstractions efficiently.
    No, I do not care if the average C hacker can't read my code.
  * Hardcoding is avoided where that makes sense, with the help of template metaprogramming.
    For example, the configuration specifies a list of heaters, and it is trivial to add new heaters.
  * Pure avr-gcc code, no reliance on inefficient libraries.

## Building it

  * Make sure you have avr-g++ 4.8.1 or newer.
    Since it's not that easy to build or get that, I provide a
    (hopefully) portable build for Linux: https://docs.google.com/file/d/0Bx9devQE0OqbWTh4VUFvOWhlNEU/edit?usp=sharing
    Sorry about the giant file size, there were some problems with stripping the binaries.
    To use the toolchain, extract it somewhere and modify your PATH as follows:
    export PATH=/path/to/toolchain/bin:$PATH
  * Edit compile.sh and adjust MCU, F_CPU and MAIN to reflect your board.
    If your compiler is not available as avr-g++, adjust CROSS appropriately.
  * Open aprinter/printer/aprinter-YourBoard.cpp and adjust the configuration.
    If you don't know what something means, you probably don't need to change it.
    All units are based on millimeters and seconds.
    NOTE: documentation of configuration parameters is present in aprinter-melzi.cpp only.
  * Regenerate the thermistor tables inside the generated/ folder to match your thermistor and resistor types.
    You can find the regeneration command inside the files themselves.
    The python script mentioned prints the code to stdout, you need to pipe it into the appropriate file.
  * Run compile.sh to compile the code.
  * Upload the code to your MCU, however you normally do that; see flash.sh for an example.
    If you're only used to uploading with the Arduino IDE, you can enable the verbose upload option in its preferences,
    and it will print out the avrdude command when you try to upload a sketch.

## Testing it

  * Connect to the printer with Pronterface, and make sure you use baud rate 57600.
  * Try homing and some basic motion.
  * Check the current temperatures (M105).
  * Only try turning on the heaters once you've verified that the temperatures are being reported correctly.
    Be aware that if you generate a thermistor table with the wrong beta value,
    the room teperature will be reported correctly, but other temperatures will be incorrect
    (possibly lower, and you risk burning the heater in that case).
    Obviously, take safety precausions here. I'm not responsible if your house burns down as a result of
    using my software, or for any other damage.

## SD card support

The firmware supports reading G-code from an SD card. However, the G-code needs to be written directly to the SD card in sequential blocks, starting with the first block.

To enable SD card support, some changes need to be done in your main file:
```
...
// add these includes
#include <aprinter/devices/SpiSdCard.h>
#include <aprinter/system/AvrSpi.h>
...
    // remove "PrinterMainNoSdCardParams," and replace it with this (don't forget the trailing comma):
    // The SsPin below is for RAMPS1.4. For Melzi, use AvrPin<AvrPortB, 4>.
    PrinterMainSdCardParams<
        SpiSdCard,
        SpiSdCardParams<
            AvrPin<AvrPortB, 0>, // SsPin
            AvrSpi
        >,
        GcodeParserParams<8>,
        2, // BufferBlocks
        100 // MaxCommandSize
    >,
...
// add this to where the other ISR macros are
AMBRO_AVR_SPI_ISRS(*p.myprinter.getSdCard()->getSpi(), MyContext())
...
```

**Additionally**, you will need to reduce your buffer sizes to accomodate the RAM requirements of the SD code. Lowering `StepperSegmentBufferSize` and `EventChannelBufferSize` to 10 should do the trick.

Once you've flashed firmware wirh these changes, you will have the following commands available:

- M21 - Initializes the SD card.
- M22 - Deinitializes the SD card.
- M24 - Starts/resumes SD printing. If this is after M21, g-code is read from the first block, otherwise from where it was paused.
- M25 - Pauses SD printing.

To print from SD card, you need to:

- Prepare your g-code file, by removing all empty lines and comments, and adding an `EOF` line. If you're using the `DeTool.py` postprocessor (TODO document this), it is sufficient to enable the SD card option there.
- Pipe this file to the SD card device node (not partition). Obviously, this will destroy any existing data on your card.
- Insert the card into the printer, then issue M21 to initialize the card, and M24 to begin printing.
