aprinter
========

APrinter is a currently experimantal firmware for RepRap 3D printers and is under heavy development.

## Implemented features (possibly with bugs)

  * Serial communication using the defacto RepRap protocol. Baud rate above 57600 will not work, for now.
  * Homing, including homing of multiple axes at the same time. Either min- or max- endstops can be used.
  * Line motion with acceleration control and cartesian speed limit (F parameter).
    Speed limit in case of E-only motion is not implemented.
    The speed is automatically limited to not overload the MCU with interrupts.
  * Look-ahead to preserve some speed on corners. By default, planning takes the last 3 moves into account
    (the most recent one plus two older moves). This can be increased in the configuration, but this
    also increases the chance of buffer underruns , which cause the print to temporarily pause while the buffer refills.
    Look-ahead is very memory-hungry in its current state.
  * Heater control using PID or on-off control. The thermistor tables need to be generated with a Python script.
  * Safe temperature range. A heater is turned off in case its temperature goes beyound the safe range.
  * Fan control.
  * Starting and feeding of the watchdog timer.
  * Emergency shutdown of motors and heaters in case of an assertion failure
    (if assertions are enabled with -DAMBROLIB_ASSERTIONS).
  * Non-drifting heartbeat LED. Is period is exactly 1 second, subject to the precision of your oscillator.

## Planned features (in the approximate order of priority):

  * Optimization of the planning and stepping code, both for speed and memory usage.
  * Runtime configurability and settings in EEPROM.
  * SD-card printing.

## Hardware requirements

I'm developing this software using my Melzi board (atmega1284p),
which has been manually clocked to 20MHz from the default 16MHz.
I expect the software to work with 16MHz as well, but the software will limit the maximum stepping speed
proportionally. Generally, any AVR satisfying the following should work,
possibly requiring minor adjustments in the code:

  * 8kB of SRAM,
  * 128kB of flash,
  * 3 timers (any one of these can be either 8- or 16-bit).

## Software requirements

You need a recent development version of GCC 4.9 targeting AVR.
Version 4.8.1 is known not to work, likely due to a bug in the compiler.
Anything earlier will be even more useless, including the ancient compilers that come with Arduino.

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
  * Edit compile.sh and adjust MCU and F_CPU (or pass them as environment variables, but you'll forget it next time).
  * Open aprinter/printer/aprinter.cpp and adjust the configuration.
    If you don't know what something means, you probably don't need to change it.
    All units are based on millimeters and seconds.
  * Regenerate the thermistor tables inside the generated/ folder to match your thermistor and resistor types.
    You can find the generation command inside the files themselves.
    The python script mentioned prints the code to stdout, you need to pipe it into the appropriate file.
  * Run compile.sh to compile the code.
  * Upload the code to your MCU, however you do that; see flash.sh for an example.

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
