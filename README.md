aprinter
========

APrinter is a portable firmware system for RepRap 3D printers and other desktop CNC devices.
It supports many controller boards based on different microcontroller families: AVR, AT91SAM (e.g.. Arduino Due), STM32F4 and Freescale MK20 (Teensy 3). A [web-based configuration system](http://www.aprinter.eu/) is used to configure the high-level features for a particular machine, but also to define the low-level configuration for supporting different controller boards.

Here is a list of the boards which are supported out of the box. This means that a predefined Board configuration is provided in the configuration editor. Note that for some of the supported microcontrollers (STM32F4, Teensy 3), there is no specific board supported. It is up to you to build a board and bring the code to life :)
- Duet (based on AT91SAM3XE).
- RADDS, RAMPS-FD (based on Arduino Due / AT91SAM3XE).
- 4pi.
- RAMPS 1.3/1.4 (based Arduino Mega / ATMEGA2560).
- Melzi (based on ATMEGA1284p).

The following machines are supported out of the box (meaning that a functional Configuration section is provided).
- RepRapPro Fisher.

## Major functionality

- Supports many geometries (in addition to Cartesian): linear-delta, rotational-delta, SCARA (like Morgan) and CoreXY. New geometries can be added by implementing a foward and inverse coordinate transformation. A processor with sufficient speed and RAM is needed (not AVR).
- Bed probing using a digital input line (e.g. microswitch). Height measurements are printed to the console.
- Bed height correction, either with a linear or quadratic polynomial, calculated by the least-squares method.
- SD card and FAT32 filesystem support. G-code can be read from the SD-card. Optionally, the SD card can be used for storage of runtime configuration options. A custom (fully asynchronous) FAT32 implementation is used, with limited write support (can write to existing files only).
- Ethernet network (currently on Duet only). Gcode console over TCP is supported (equivalent to the serial-port interface), with multiple concurrent connections. Pronterface can connect this way.
- Supports heaters and fans. Any number of these may be defined, limited only by available hardware resources.
- Experimental support for lasers (PWM output with a duty cycle proportional to the current speed).
- Supports multiple extruders. However, the interface is not compatible to typical firmwares. There are no tool commands, instead the extruders appear as separate axes. A g-code post-processor is provided to translate tool-using gcode into what the firmware understands. This post-processor can also control fans based on the "current tool".
- Unified runtime configuration system. Most of the "simple" configuration values which are available in the configuration editor have the corresponding named runtime configuration option (e.g. XMaxPos). Configuration may be saved to and restored from a storage backend, such as an EEPROM or a file on an SD card.

## Other features and implementation details

- Homing of multiple axes in parallel.
- Homing cartesian axes involved in a coordinate transformation (e.g. homing X and Y in CoreXY).
- Multiple steppers, driven synchronously, can be configured for an axis (e.g. two Z motors driven by separate drivers).
- Constant-acceleration motion planning with look-ahead. To speed up calculations, the firmware will only calculate a new plan every N ("Lookahead commit count") commands. This allows increasing the lookahead without an asymptotic increase of CPU usage, only limited by the available RAM.
- High precision step timing. For each stepper, a separate timer compare channel is used. In the interrupt handler, a step pulse for a stepper is generated, and the time of the next step is calculated analytically.
- Multiple serial ports can be configured (e.g. both UART and USB CDC), assuming drivers are written.

## Coding style

  * Extreme attention to detail and bugless code. Lots of assertions (the proven kind, not guesses).
  * The software is written in C++11, with some use of G++ specific features such as constexpr math.
  * Extensive use of abstractions for hardware access and program structuring.
    A dependency injection pattern is used to decouple many components from their dependencies
    where appropriate.  Assembly is performed in the main file.
  * Template metaprogramming is used to implement the abstractions efficiently.
    No, I do not care if the average C hacker can't read my code.
  * Hardcoding is avoided where that makes sense, with the help of template metaprogramming.
    For example, the configuration specifies a list of heaters, and it is trivial to add new heaters.
  * No reliance on inefficient libraries such as Arduino.

## APrinter Web Service

The easiest way to get started is using the public web service,
available at [www.aprinter.eu](http://www.aprinter.eu/).
The service allows you to configure the firmware, and will build it at your request.
It should work with any modern web browser, and the only step that needs to be done
locally is uploading the firmware to the board.
If you want to, you can run the service locally, as described next.

A prerequisite for running locally is the [Nix package manager](http://nixos.org/nix/) running on Linux.
If you're not familiar with Nix, please use the installer, not distribution packages.

After you perform the commands below, the service will be available at `http://127.0.0.1:4000/`.

```
nix-build nix/ -A aprinterService -o ~/aprinter-service
mkdir ~/aprinter-service-temp
~/aprinter-service/bin/aprinter-service
```

Instructions for using the web service:
- Define a "configuration" - either modify or copy an existing configuration.
- Make changes to the configuration.
- Select this configuration in the combo box at the top.
- At the bottom of the page, press the compile button.
- Wait some time for the compilation to complete. You will receive a zip with the firmware image.
- Extract the zip and upload the firmware image to your board.

## Building manually

It is possible to build without using the web service, given a JSON configuration file.
This file would usually be produced by the web GUI, but you're free to manage it manually.
Again, the prerequisite for building is the [Nix package manager](http://nixos.org/nix/) on Linux.
If you're not familiar with Nix, please use the installer, not distribution packages.

```
python -B config_system/generator/generate.py --config path_to_config.json | nix-build - -o ~/aprinter-build
```

## Uploading

Before you can upload, you need to install the uploading program, which depends on the type of microcontroller:
- AVR: avrdude (install with `nix-env -i avrdude`).
- Atmel ARM: BOSSA (intall with `nix-env -i bossa`).
- Teensy 3: teensy-loader (install with `nix-env -i teensy-loader`).

There is a Python program included in the root of the source that will do the upload using the appropriate tool.
It is generally used like this:

```
python -B flash.py -t <board-type> -f <file-to-flash> [-p <port>]
```

Below, the specific command used to flash manually are also shown.

### RAMPS
```
avrdude -p atmega2560 -P /dev/ttyACM0 -b 115200 -c stk500v2 -D -U "flash:w:$HOME/aprinter-build/aprinter-nixbuild.hex:i"
```

### Melzi

You will have to set the Debug jumper and play with the reset button to get the upload going.

```
avrdude -p atmega1284p -P /dev/ttyUSB0 -b 57600 -c stk500v1 -D -U "flash:w:$HOME/aprinter-build/aprinter-nixbuild.hex:i"
```

### Arduino Due

Make sure you connect via the programming port while uploading (but switch to the native USB port for actual printer communication).

First you need to set baud rate to 1200 to start the bootloader:

```
stty -F /dev/ttyACM0 1200
```

Then upload the firmware using BOSSA (you can use the GUI if you like instead).

```
bossac -p ttyACM0 -U false -i -e -w -v -b ~/aprinter-build/aprinter-nixbuild.bin -R 
```

Some Due clones have a problem resetting. If after uploading, the firmware does not start (LED doesn't blink), press the reset button.

For communication with host software (not programming), the software supports both the programming and native USB ports at the same time. The programming port is by default configured at baud rate 115200, because 250000 does not work on all Due boards due to a design defect. It is recommented to use the native USB port due to greater speed.

### Duet

Before flashing, you need to bring the chip to boot mode by pressing the erase button (near the Ethernet jack). If the board does not reset after flashing (despite us telling it to reset, go figure), you will have to power cycle.

```
bossac -p ttyACM0 -i -e -w -v -b ~/aprinter-build/aprinter-nixbuild.bin -R
```

### Teensy 3

You need to press the button on the board before trying to upload, to put the board into bootloader mode.

```
teensy_loader_cli -mmcu=mk20dx128 "$HOME/aprinter-build/aprinter-nixbuild.hex"
```

## Feature documentation

Different features of the firmware are described in the following sections.

Note that the ability to define new devices (heaters, fans, extruders...) depends on the particular board/microcontroller.
In the web GUI, if there is an available "port" (stepper port, PWM output),
it is just a matter of adding a device to the Configuration section.
Otherwise it may or may not be possible to add a new port, based on hardware availability (timer units, PWM channels).

### Runtime configuration

When runtime configuration is available, the values of many parameters can be adjusted at runtime, and the values specified in the web GUI are used as defaults only. Notable things that cannot be configured at runtime are various structural aspects such as the presence of devices/features and hardware mapping of features (including pins).

Board-specific notes:

  * Duet, RADDS, RAMPS-FD, 4pi, Melzi: SD card is used for configuration storage.
  * RAMPS: EEPROM in the microcontroller is used.

Runtime configuration commands:

  * Print current configuration: `M503`.
  * Get option value: `M925 I<option>`
    Example: `M925 IXMin`
  * Set option value: `M926 I<option> V<value>`
    Example: `M926 IXMin V-20`
    For boolean option, the values are 0 and 1.
    Omit the V argument to set the option to the default value.
  * Set all options to defaults: `M502`
  * Load configuration from storage: `M501`
  * Save configuration to storage: `M500`
  * Apply configuration: `M930`

After changing any configuration (e.g. with `M926`, `M502` or `M501`), the configuration needs to be applied with `M930`. Only then will the changes take effect. However, when the firmware starts up, the stored configuration is automatically loaded and applied, as if `M501` followed by `M930` was done.

The `M930` command does not alter the current set of configuration values in any way. Rather, it recomputes a set of values in RAM which are derived from the configuration values. This is a one-way operation, there is no way to see what the current applied configuration is.

If configuration is stored on the SD card, the file `aprinter.cfg` in the root of the filesystem needs to exist. The firmware is not capable of creating the file when saving the configuration! An empty file will suffice.

### Error handling

The firmware understands the concept of failed commands. The conditions for failure are command-specific.

This error information is used by a special feature which prevents executing commands after one command fails:

- M932 - Clears error status and enables refuse-after-error mode. After this, when a command fails, further commands will immediately be rejected.
- M933 - Disables refuse-after-error mode.

This feature is intended to be used when printing via serial or TCP - you can put M932 at the top of the program, and M933 at the end. Please note if you do this and an error appears, your host software probably won't be able to recover, but will just send the rest of all the gcodes which will fail. If you want to use this feature and have the option to recover, you should implement this in your host software :)

However, some host software will itself stop sending commands when an error is returned in one of the commands. This works fine when the host waits for each "ok" before sending the next command. But if you want to stream commands (presumably over TCP), the use of M932/M933 is essential for stopping at the first error.

### SD card

The firmware supports reading G-code from a file in a FAT32 partition on an SD card.
When the SD card is being initialized, the first primary partition with a FAT32 filesystem signature will be used.

There is partial write support; existing files can be written but new files cannot be created. Write support can be utilized for uploading G-code (M28, M29) and for storing the configuration (see the Runtime Configuration section).

**WARNING**: Back up any important data on the SD cards you would be using with the device. Data loss is possible, e.g. due to bugs in the SD card driver and the FAT filesystem code.

SD card support is enabled by default on the following boards: Duet, RADDS, RAMPS-FD, 4pi, RAMPS 1.3, Melzi. Write support is enabled on Duet, RADDS, RAMPS-FD, 4pi, Melzi. Note, the ATMEGA2560 has too little RAM for enabling write support.

The following SD-card related commands are implemented:

- M21 - Initialize the SD card. On success the root directory will become the current directory.
- M22 - Deinitialize the SD card.
- M20 - List the contents of the current directory.
- M23 D\<dir\> - Change the current directory (<dir> must be a directory in the current directory, or ..).
- M23 R - Change to root directory.
- M23 F\<file\> - Select file for printing (<file> must be a file in the current directory).
- M32 F\<file\> - Select file and start printing.
- M24 - Start or resume SD printing.
- M25 - Pause SD printing. Note that pause automatically happens at end of file.
- M26 - Rewind the current file to the beginning.
- M28 F\<file\> - Start writing commands to a file (in the current directory).
- M29 - Stop writing commands to file.

When passing a file or directory name to a command, any spaces in the name have to be replaced with the escape sequence `\20`, because a space would be parsed as a delimiter between command parameters.

Note, currently Pronterface's SD button does not work with Aprinter, you need to use the SD commands directly.

When printing from SD card, the firmware will stop on the first failed command (the error will be printed to the console). You can resume at the next command using M24.

Example: start printing from the file `gcodes/test.gcode`.

```
M21
M23 Dgcodes
M32 Ftest.gcode
```

Example: interrupt and restart the same print.

```
M25
M26
M24
```

Example: repeat a successful print.

```
M26
M24
```

G-code can be uploaded using the commands M28 and M29. You should send M28, then send all the gcode to be written to the file (you can just tell Pronterface to "print"), then send M29. Alternatively, you can put M28/M29 into the start/end gcode in your slicer's settings. Please make sure that the file exists, the firmware currently cannot create new files, only overwrite existing ones.

Futher, to avoid accidentally executing the commands in case opening the file fails, you should wrap the whole thing in M932/M933.

```
M932
M28 Fprint.gcode
// Send your gcode.
M29
M933
```

### Networking

On the Duet board, Ethernet networking is supported. Currently, the only network service provided is a Gcode console over a TCP connection.

By default, DHCP will be used to acquire an IP address automatically. You can run the command `M940` to see the assigned IP address and other network status information. Alternatively, you can configure a static IP, as shown. Don't forget to run `M930` and/or `M500` to apply/save this configuration (see the Runtime Configuration section).

```
M926 INetworkDhcpEnabled V0
M926 INetworkIpAddress V192.168.1.234
M926 INetworkIpNetmask V255.255.255.0
M926 INetworkIpGateway V192.168.1.1
M930
```

The TCP console will be available on port 23. You tell Pronterface to connect to this TCP interface by entering `<ip_address>:23` into the Port box. By default, two concurrent connections are permitted.

### Heaters

Each heater is identified with a one-character name and a number. In the configuration editor, the number may be omitted, in which case it is assumed to be zero. On the other hand, the firmware will assume the number 0 if a heater is specified in a heater-related command with just a letter. Typical heater names are B (bed), T/T0 (first extruder) and T1 (second extruder).

To configure the setpoint for a heater and enable it, use `M104 <heater> S<temperature>`. For example: `M104 B S100`, `M104 T S220`, `M104 T1 S200`. To remove the setpoint (disabling the heater), set it to nan (or a value outside of the defined safe range): `M104 B Snan`.

The command M116 can be used to wait for the set temperatures of heaters to be reached: `M116 <heater> ...`. For example: `M116 T0 T1 B`. Without any known heaters specified, the effect is as if all heaters with configured setpoints were specified. The command will fail immediately if a heater which is explicitcly specified does not have a setpoint configured.

The firmware detects thermal runaways, when the temperature falls outside the defined safe range. Upon runaway, the specific heater is automatically disabled, and an error message is generated. The command `M922` can be used to re-enable heaters which had experienced a thermal runaway. Note, a heater being disabled due to a thermal runaway does not change its setpoint - this is implemented such to provide predictable semantics of M116.

Optionally, heater-specific M-codes can be defined in the configuration editor. For example is M123 is configured for the heater `T1`, the command `M123 S<temperature>` is equivalent to `M104 T1 S<temperature>`. Note, `M104` itself may be configured as a heater-specific M-code. In this case `M104` may still be used to configure any heater, but if no heater is specified, it configures that particular heater. It is useful to configure `M140` as the heater-specific code for the bed, and `M104` for an only extruder.

### Fans (or Spindles)

Fans are identified in much the same way as heaters, with a letter and a number. For fans attached to extruders, the names T/T0/T1 are also recommended.

A fan is turned on using `M106 <fan> S<speed_0_to_255>`. Turning off is achieved either by setting the speed to 0 or using `M107 <fan>`.

Much like heater-specific M-codes, one can have fan-specific M-codes for setting the speed of or turning off a fan. If there is only one fan, it is useful to use `M106` and `M107` themselves as heater-specific M-codes for the fan.

You can also use this feature for simple PWM-control of spindles or anything else; for spindles you may map the on- and off-commands to M3 and M5 respectively.

### Extruders

The firmware allows any number of axes (given sufficient hardware resources), but it does not, by design, implement tool change commands. Extruder axes need to be configured with the "is cartesian" option set to false.
This distinction is required for the implementation of the feedrate parameter (`G1 Fxxx ...`).

The recommented naming for extruder axes is E, U, V in order.

The included `DeTool.py` script can be used to convert tool-using g-code to a format which the firmware understands, but more about that will be explained later.

### Multiple steppers per axis

More than one stepper can be assigned to an axis, and they will be driven synchronously.

In the web GUI, additional steppers can be added in the Stepper section for a particular stepper.
If there is no existing suitable stepper port definition, you will need to add one in the Board configuration.
When doing this, note that for the additional (non-first) steppers in an axis, the "Stepper timer" does not need to be defined (you may set it to "Not defined").

### Coordinate transformation

APrinter has generic support for non-cartesian coordinate systems.
Currently, linear delta, rotationa delta, SCARA and CoreXY/H-bot are supported out of the box.

*NOTE*: The nonlinear transforms will not work well on AVR based platforms due to lack of CPU speed and RAM. CoreXY may or may not, depending on how many axes (esp. extruders) you configure.

Generally, the transformation is configured as follows in the web GUI:

- In the Steppers list, define the steppers (actuators) that are involved in the transform,
  with names that are *not* cartesian axes (linear/rotational delta: A, B, C, CoreXY/SCARA: A, B).
  Ensure that for each such stepper, "Is cartesian" is set to No.
  Set the position limits correctly (see the sub-section next).
  Note that for rotational delta and SCARA, the positions of these axes are measured in degrees.
  For delta, enable homing for all three steppers, for CoreXY disable homing for both.
- Define any remaining Cartesian steppers. So for CoreXY and SCARA that would be Z, and for linear/rotational delta none.
  Of course extruders still belong to the Steppers list.
- In the "Coordinate transformation" section, select your particular transformation type.
  More configuration is made available.
- Set up the "Stepper mapping" to map the physical axes (actuators) defined earlier.
  Type the axis name (A, B, C) to map an axis.
  Note that you are free to define the mapping in any order, to achieve correct motion.
  Which is a bit tricky woth CoreXY - you may also have to invert stepper direction.
- Set the transformation-type specific parameters. The delta-related parameters mean exactly
  the same as for Marlin, so no further explanation will be given here.
- Configure the segmentation. You have to enable segmentation if you use a nonlinear geometry
  (all but CoreXY). Note that when performing segmentation, the firmware first calculates an initial
  number of segments based on the desired speed of a move and the segments-per-second setting,
  than clamps this value to the limits obtained based on the configured minimum and maximum segment length.
- Configure the cartesian axes. Currently this is just position limits and maximum speed.
  But, for CoreXY, you have the option of enabling homing for cartesian axes.

#### Stepper configuration

Steppers involved in the coordinate transform (typically A, B and maybe C) have their own configuration which is generally the same as for other steppers.
In particular they need to be defined the position limits.

For CoreXY, the position limits should not constrain motion as defined by the limits of the cartesian axes.
The following formulas give the minimum required stepper limits to support the cartesian limits
(assuming no reordering of steppers): Arange = [Xmin + Ymin, Xmax + Ymax], Brange = [Xmin - Ymax, Xmax - Ymin].

For delta, the stepper limits should be configured appropriately for the machine.
It helps to know that the A/B/C stepper positions are actually cartesian Z coordinates
of assemblies on the towers, and that the maximum position limit corresponds to
meeting the endstop at the top.

For rotational delta and SCARA, these steppers use units of degrees, not millimeters.
You should not get confused when the configuration editor mentions millimeters.
For example, the position limits are degrees, speeds (accelerations) are degrees per second (squared), and steps-per-unit are steps per degree.

### Bed probing and correction

When bed height probing is enabled, a list of corrdinates of the probe points needs to be defined, along with other parameters such as starting height and speeds. The configuration editor should provide sufficient information for configuring bed probing. Bed probing is initiated using `G32`. The resulting height measurements will be printed to the console.

Automatic height correction is supported. However, due to design, a precondition is that the X, Y and Z axes are involved in the coordinate transformation. On delta machines, this is obviously satisfied, however, on Cartesian or CoreXY machines, special configuration is needed (see below).

Correction may be either linear or quadratic. If quadratic correction is enabled in the configuration editor, a runtime configuration parameter (ProbeQuadrCorrEnabled) switches between linear and quadratic. Note, a correction is always applied on top of the existing correction, and a linear correction on top of a quadratic correction is still a quadratic correction.

In any case, the computation of corrections is done using the linear least-squares method, via QR decomposition by Householder reflections. Even though this code was highly optimized for memory use, it still uses a substantial chunk of RAM, so you should watch out for RAM usage. The RAM needs are proportional in the number of probing points.

An important setting for calibration is the "Z offset added to height measurements", and the associated `ProbeGeneralZOffset` runtime configuration option. When a point is probed:
- If the probe is triggered a certain distance before the nozzle touches the bed (the nozzle does not reach the bed), this should be set to minus that distance.
- If the nozzle needs to push down into the bed to trigger the probe, this should be set to this positive push distance.

It is possible to specify point-specific Z offsets; the general and point-specific offset are added to produce the effective Z offset. This allows compensating for the elasticity of the bed (in designs where this is needed).

#### Configuring probing for Cartesian machines

First do the basic configuration of axes:
- Rename the X, Y, Z steppers to A, B, C, and set "Is cartesian" to No for all three.
- Select "Coordinate transformation": Identity.
- Add three identity axes, named X, Y, Z, for steppers A, B, C respectively. For X, Y select "Position limits": "Same as stepper".
- For Z, select "Position limits": Specified, and set the limits. You surely want to use 0 as the minimum.

You also must make sure the limits and homing for the C (Z) stepper are set correctly. Generally, the C/Z position should be sufficiently precise after homing so that probing works correctly. If you have a min-endstop on Z, it is advised to configure as follows:
- Physically calibrate the Z endstop so that when homed, the nozzle is (slightly) above the bed at all XY coordinates.
- From the homed position, the endstop must allow sufficient movement downward so that the nozzle can reach the bed at every XY coordinate, without the axis colliding into the endstop.
- The "Minimum position" and "Offset of home position" settings for the C stepper should be tuned to achieve the above requirements. An example setting is -1mm and 2mm, which puts the axis to logical position 1mm (=-1mm+2mm) when homed, and allows it to move an additional 2mm downward to position -1mm.

### Maximum speed

There are different ways to specify the maximum speed of a move. The following rules apply, in order.
Please note that the notion of some axis appearing in a move command is literal regardless of the amount of motion,
e.g. E does appear in "G1 E0" while X does not.

- If a T parameter appears in the move command, the T value will be understood as the nominal move time in seconds.
  This means that speed will be limited to the speed at which the move would be performed if took exactly T seconds
  at constant speed. So, the move will take at least T seconds but may take longer (e.g. due to accelerations).
- If at least one Cartesian axis appears in the move command, the last seen F value will be used to limit the
  Euclidean speed of the move, based on the Euclidean distance calculated from all Cartesian axes.
- Otherwise (no T parameter, no Cartesian axes - e.g. extruders only), the last seen F value will be used to limit
  the speed of each individual stepper axis (and not virtual axes).

Note that the desired speed as described above is only understood as a nominal value.
The machine will not move faster than that, but it may move slower.
For example the software will still respect the configured maximum accelerations and maximum speeds of the actuator axes.

The speed is also bound to not exceed the capability of the processor.
The configuration parameter `MaxStepsPerCycle` controls this limit; it is available in the Board configuration section under Performance parameters, and also as a runtime setting.
The firmware will ensure that the cumulative step frequency (across all actuator axes) does not exceed the frequency of the processor multiplied by `MaxStepsPerCycle`.

If you are aiming for high step rates , check that the firmware is being compiled without size optimization (under Board, Performance parameters) and with assertions disabled (under Board, Development features).

### Lasers

There is currently experimental support for lasers, more precisely,
for a PWM output whose duty cycle is proportional to the current speed.
The laser configuration parameters are:

- `LaserPower` [W]: The actual power of the laser at full duty cycle.
  You don't strictly have to measure the power, this just serves as a
  reference for everything else, you can use e.g. 1 or 100.
- `MaxPower` [W]: An artificial limit of laser power. Use =`LaserPower` to
  allow the laser to run at full duty cycle. But make sure it's not
  greater than `LaserPower`, that will result in strange behavior.
- `DutyAdjustmentInterval` [s]: Time interval for duty cycle adjustment
  from a timer interrupt (=1/frequency).

A prerequisite for configuring a laser is the availability of hardware PWM and its support by the firmware. See the section "Hardware PWM configuration" for more details.

A laser is configured in the web GUI as follows:

- In the Board section, add a "Laser port". Give it a name (anything), select the PWM output.
  This must be a hardware PWM output.
  Also select an available timer unit. This is not exactly easy since the timer allocations
  are spread throughout the Board configuration (Event channel timer, stepper timers,
  timers for software PWM outputs).
- In the Configuration section, add a Laser. The default values here should be usable.
  But make sure your new "Laser port" is selected.

In the g-code interface, *either* of the following parameters can be used in a `G0`/`G1` command to control the laser:
- **L** - Total energy emmitted over the segment [W]. This is the low level interface to the laser.
- **M** - Energy density over the segment [W/mm]. The value is multiplied by the segment length to obtain the effective energy for the segment.

The energy density *M* is cached, so you can specify it in one command, and leave it out for any further commands where you want the same energy density.
If *L* is specified, it takes precedence over *M* or its cached value, but it does not alter the *M* cached value.

## The DeTool g-code postprocessor

The `DeTool.py` script can either be called from command line, or used as a plugin from `Cura`.
In the latter case, you can install it by copying (or linking) it into `Cura/plugins` in the Cura installation folder.

To run the script, you will need to provide it with a list of physical extruders, which includes the names of their axes,
as understood by your firmware, as well as the offset added to the coordinates.
Futher, you will need to define a mapping from tool indices to physical extruders.
The command line syntax of the script is as follows.

```
usage: DeTool.py [-h] --input InputFile --output OutputFile
                 --tool-travel-speed Speedmm/s --physical AxisName OffsetX
                 OffsetY OffsetZ --tool ToolIndex PhysicalIndexFrom0
                 [--fan FanSpeedCmd PhysicalIndexFrom0 SpeedMultiplier]
```

For example, if you have two extruder axes, E and U, the U nozzle being offset 10mm to the right, and you want to map the T0 tool to U, and T1 to E,
you can pass this to the script:

```
--physical E 0.0 0.0 0.0 --physical U -10.0 0.0 0.0 --tool 0 1 --tool 1 0
```

The argument `--tool 0 1` means to associate T0 to the second physical extruder (with the letter E, in this case).\
Note that the offset in `--physical` is what the script adds to the position in the source file, that is, the negative of the nozzle offset.

The script also processes fan control commands (M106 and M107) to appropriate fans based on the selected tool.
For example, if your first fan (M106) blows under the first extruder, and the second fan (M406) under the second extruder,
you will want to pass:

```
--fan M106 0 1.0 --fan M406 1 1.0
```

If the fans are not equally powerful, you can adjust the `SpeedMultiplier` to scale the speed of specific fans.

## Support

If you need help or want to ask me a question, you can find me on Freenode IRC in #reprap (nick ambro718),
or you can email me to ambrop7 at gmail dot com. If you have found a bug or have a feature request, you can use the issue tracker.
