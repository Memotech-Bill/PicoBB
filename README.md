# PicoBB - Pico BBC BASIC

> [!IMPORTANT]
> **19 December 2024**
> The folder layout and build instructions have changed to better support Pico2.
> Please read the details below before attempting to build.

For the original BBCSDL please go to https://github.com/rtrussell/BBCSDL

This project is part of an attempt to implement BBC Basic on a Raspberry Pi Pico.
It was originally a fork of R. Russell's repository, but now that is included as
a sub-module.

For discussion see https://www.raspberrypi.org/forums/viewtopic.php?f=144&t=316761

It includes work by:

* R. T. Russell
* Eric Olson
* Myself

Apologies to anyone else I omitted.

There are currently standard builds for four hardware configurations,
each available in versions for the RP2040 (Pico) or RP2350 (Pico2):

### Pico Console version

* Intended to be able to use the Pico as a microcontroller programmed in BBC Basic.
* Runs on a bare Pico, or attached to custom hardware to be controlled 
* User interaction (if any) is via a console interface over USB or serial.
* Includes program storage in Pico flash.
* Support for WiFi networking if a CYW43 chip included (Pico_W or Pico2_W).
* High quality sound (as per BCSDL) is available as PWM from two GPIO pins.

### VGA version

* Uses the Pico as a computer programmable in BBC Basic with input by an attached USB keyboard,
  and display on an attached VGA monitor.
* Designed to run on a Pico attached to a VGA demonstration board as per chapter 3 of
  [Hardware design with RP2040](https://datasheets.raspberrypi.org/rp2040/hardware-design-with-rp2040.pdf)
  or the commercial version
  [Pimoroni Pico VGA Demo Base](https://shop.pimoroni.com/products/pimoroni-pico-vga-demo-base).
* Includes program and data storage either in Pico flash or on the SD card on the VGA demo board.
* Support for WiFi networking if a CYW43 chip included (Pico_W or Pico2_W).
* Sound is low quality (as per BBC Micro) available via the PWM output.

### PicoCalc version

* Uses the Pico as a hand-held computer programmable in BBC Basic with input and output on the PicoCalc
  keyboard and display.
* Designed to run on a Pico installed in a [PicoCalc](https://www.clockworkpi.com/picocalc).
* Includes program and data storage either in Pico flash or the PicoCalc SD card.
* Support for WiFi networking if a CYW43 chip included (Pico_W or Pico2_W).
* High quality sound (as per BCSDL) via the built-in speakers or headphone socket.

### Pico-ResTouch-LCD-3.5 version

* Uses the Pico to implement a touch-controlled user interface, or as a graphical display
  for user interaction over a USB interface.
* Designed to run on a Pico attached to a Waveshare
  [Pico-ResTouch-LCD-3.5](https://www.waveshare.com/wiki/Pico-ResTouch-LCD-3.5) display.
* Includes program and data storage either in Pico flash or an SD card on the Waveshare board.
* Support for WiFi networking if a CYW43 chip included (Pico_W or Pico2_W).

Many other custom builds are available by specifying options on the make command line.

The following limitations are noted:

1.  Memory is limited. Complex expressions or deeply nested routines may result in the CPU stack
    reaching HIMEM or any INSTALLed libraries or refresh buffers. Usually this will result in either
    "Expression evaluation too deep!" or "Recursion too deep!" error message. It may be possible to
    resolve this by lowering HIMEM. It may, however, be possible for the situation to go unnoticed
    in which case the Pico may crash and have to be reset.

2.  Programs in tests and the root filesystem have been tested and appear to work.
    However, any remaining bugs are more likely to be related to the Pico port. As always
    this is open source with expressly no warranty provided.

3.  There is are known buffer overflows in the wrappers appearing in lfswrap.c which are
    triggered when a filename path grows to be greater than 256 characters. Please don't do that.

This project includes source from various locations with difference licenses. See the
various LICENSE.txt files.

## Build Instructions

Building has been tested on a Raspberry Pi running Rasperry Pi OS. Building in other
environments may present difficulties.

To build this for the Pico, make sure you have the SDK installed and the tinyusb module installed.
Building the VGA version requires [pico-extras](https://github.com/raspberrypi/pico-extras)
as well as [pico-sdk](https://github.com/raspberrypi/pico-sdk). Set the environment variables
PICO_EXTRAS_PATH and PICO_SDK_PATH respectively to the locations where these are installed.
Installing the SDK using the script [pico_setup.sh](https://raw.githubusercontent.com/raspberrypi/pico-setup/master/pico_setup.sh)
will automatically install and configure everything required.

Download the source code using:

     git clone --recurse-submodules https://github.com/Memotech-Bill/PicoBB.git

Ensure that the environment variable PICO_SDK_PATH points to the path where the SDK is installed.

### Console Versions

Select the console/pico folder then type one of:

     make BOARD=pico
     make BOARD=pico_w
     make BOARD=pico2
     make BOARD=pico2_w

Note that there is little advantage in building the non WiFi versions. The with WiFi versions of
the software will run on a board without the WiFi chip (although clearly networking will not work).
The without WiFi software version does make a very small amount of extra memory available to BASIC.

To build a version for a third-party (not Raspberry Pi) board, use the command:

     make BOARD=<board_name> TYPE=<pico_type>

where <board_name> is the name of the third-party board, and <pico_type> selects the basic properties
as per the following table:

| pico_type | Processor | Has CYW43 chip |
|:---------:|:---------:|:--------------:|
|   pico    |  RP2040   |       N        |
|  pico_w   |  RP2040   |       Y        |
|   pico2   |  RP2350   |       N        |
|  pico2_w  |  RP2350   |       Y        |

Having completed the make, the following files should be in the folder:

* bbcbasic_console_xxx.uf2 - The BBC BASIC interpreter.
* filesystem_console_xxx.uf2 - An image for the flash file system.
* bbcbasic+filesystem_console_xxx.uf2 - A combination of the above two.

where "_xxx" is replaced by the name of the selected board.

### VGA Versions

The GUI version has been designed to run on a Pico attached to a VGA demonstration board as per
chapter 3 of
[Hardware design with RP2040](https://datasheets.raspberrypi.org/rp2040/hardware-design-with-rp2040.pdf)
or the commercial version
[Pimoroni Pico VGA Demo Base](https://shop.pimoroni.com/products/pimoroni-pico-vga-demo-base).

The Pico SDK provides a single board definition file (vgaboard.h) for this configuration.
However this does not completely describe all the hardware configurations. Therefore this
repository includes three custom board definition files (in the boards/ folder):

#### vgaboard_sd

As supplied from Pimoroni, with no soldering, the Pico on the VGA board may be used with:

* VGA display
* USB port
* Sound via either I2S or PWM
* SD card in 4-bit, 1-bit or SPI modes.

However there is no UART serial connection. This configuration is described by the
vgaboard_sd configuration file.

This needs to `undef` the default UART. However this file is processed before the one defining
the fitted pico board. Therefore this file sets the default uart and pins to -1, and modified
versions of the `pico*.h` files have been created, which undefines the UART values if set to -1.

If using third-party RP* boards on a vgaboard it will probably be necessary to similarly
modify the board file for the third-party board.

#### vgaboard_serial

If a 3x2 header is soldered to the VGA board at the location labelled "SD Jumpers", then three
of the pins may be used to connect to one of the Pico UARTs. However with a serial device is
connected here the SD card can no longer be used. This configuration is described by the
vgaboard_serial configuration file.

If the header is solderd in, but no serial device is attached then the configuration is
still described by the vgaboard_sd configuration file.

#### vgaboard_cut

If, in adddition to attaching the 3x2 header, the tracks joining the pads GP20 to SD01 and
GP21 to SD02 are cut, then it is possible to use both the UART and the SD card, but the SD
card can only be used in 1-bit or SPI modes. This configuration is described by the
vgaboard_cut configuration file.

If jumpers are used to reconnect GP20 to SD01 and GP21 to SD02, then 4-bit SD mode may
be used and this configuration is described by the original vgaboard_sd configuration file.


Any of the vgaboard configurations may have any of the Pico types (or third party boards)
installed. This results in a potentially large number of different board definitions,
A couple of issues ([2112](https://github.com/raspberrypi/pico-sdk/issues/2112) and
[2114](https://github.com/raspberrypi/pico-sdk/issues/2114)) with the v2.0 SDK prevents
the use of nested include files to separately describe the processor board and the board
it is attached to. Therefore a way has been found to separate these definitions. The BOARD
parameter is used to specify the Pico processor board and ADDON is used to specify the base
board that provides pin connections for video, sound, SD card and serial.

Thus to build a GUI version use:

        cd bin/pico
        make BOARD=<pico_type> ADDON=<base_name>

Where <pico_type> is one of pico, pico_w, pico2 or pico2_w, and <base_name> is one of
vgaboard_sd, vgaboard_serial or vgaboard_cut (or another file specifying the pi connections).

For third-party processor boards, identify the actual board using BOARD and specify the chip
and WiFi using TYPE as for the console builds above, so that the commands to build are:

        cd bin/pico
        make BOARD=<board_name> TYPE=<pico_type> ADDON=<base_name>

Having completed the make, the following files should be in the folder:

* bbcbasic_gui_ppp_aaa.uf2 - The BBC BASIC interpreter.
* filesystem_gui_ppp_aaa.uf2 - An image for the flash file system.
* bbcbasic+filesystem_gui_ppp_aaa.uf2 - A combination of the above two.

where "ppp" is replaced by the name of the processor board and "aaa" by the name of the addon board.

### PicoCalc

A build to support the [PicoCalc](https://www.clockworkpi.com/picocalc) keyboard and display.

The PicoCalc provides:

* 320 320 LCD panel.
* 67 key calculator style keyboard.
* SD card for storage.
* Sound via built-in speakers or headphone socket.

To build the version of PicoBB for the Waveshare Pico-ResTouch-LCD-3.5:

        cd bin/pico
        make BOARD=<pico_type>  ADDON=PicoCalc GRAPH=PicoCalc STDIO=PicoCalc SOUND=SDL FAT=Y SERIAL_DEV=2
        
Where <pico_type> is replaced by the type of Pico being used.

### LCD Build

A build to support the Waveshare
[Pico-ResTouch-LCD-3.5](https://www.waveshare.com/wiki/Pico-ResTouch-LCD-3.5).

This board provides:

* 320 x 480 LCD panel - Supported by both Portrait and Landscape display modes.
* Resistive touch panel - Supported by MOUSE and ON MOUSE commands.
* Mico SD card slot - Provides additional storage for both programs and data.

This Waveshare board does not have a separate power socket, so power has to be applied via
the Pico USB socket. This means that it is not possible to attach a USB keyboard to produce
a stand-alone computer without modifying the hardware to provide a power input.

This build is therefore programmed connected to a host computer with both power and user
console supplied by the Pico USB socket. However, it is possible to write an auto-run program
which uses the LCD screen and touch panel for user interaction. This can then be run stand-alone
with power supplied via the Pico USB socket and no host computer.

To build the version of PicoBB for the Waveshare Pico-ResTouch-LCD-3.5:

        cd bin/pico
        make BOARD=<pico_type> ADDON=wslcd35 GRAPH=wslcd35 STDIO=USB SOUND=N FAT=Y

Where <pico_type> is replaced by the type of Pico being used.

### Console Version with VGA Graphics

By request an option has been added in which the BBC BASIC console is over the
USB or UART serial connection, but which is able to display text and graphics
on a separate VGA display.

This capability is not included in any of the standard builds. To enable this
feature build using a command of the form:

````
cd console/pico
make BOARD=<board_name> ADDON=<base_name>  STDIO=USB SERIAL_DEV=0 GRAPH=Y SOUND=<snd_type> SUFFIX=_<suffix>
````

where:

* `<board_name>` = One of pico, pico_w, pico2 or pico2_w. If using a third party board
  then specify its name here and also include `TYPE=<pico_type>`.
* `<base_name>` = One of vgaboard_sd, vgaboard_serial, vgaboard_cut or a custom board
  definition specifying the pin usage.
* `<snd_type>` = One of PWM or I2S.
* `<suffix>` = A string to append to the name of the build folder and file names to
  identify the build.

### Filesystem

A small collection of example BBC BASIC programs are also provided. Some of these will work
for any build, while others require features specific to a particular build. The files may be
loaded into an LFS filesystem on the Pico, or copied onto an SD card where supported.

To obtain all the example files, in either `console/pico` or `bin/pico` folders run one of
the following commands:

* `make build_filesystem` to create the `build_filesystem` folder containing all the example files.
* `make filesystem.zip` to create `filesystem.zip` containing all the example files.
* `make BOARD=pico filesystem` to create `filesystem_pico.uf2` to load the example files in
  an LFS filesystem onto a Pico or Pico_W.
* `make BOARD=pico2 filesystem` to create `filesystem_pico2.uf2` to load the example files in
  an LFS filesystem onto a Pico2 or Pico2_W.

Alternately, a combined UF2 containing both the BBC BASIC interpreter and LFS filesystem can be
created. To do this, append the build target `bbcbasic+filesystem` to any of the `make` commands
given in the previous sections. In this case only examples appropriate to the particular build
are selected.
        
## Installation

To install, plug a Pico device into the USB port while holding the boot button and then copy the
required file onto the device using a command such as:

     `cp -v bbcbasic_console_pico_w.uf2 /media/pi/RPI-RP2`

or:

     `picotool install -f bbcbasic_console_pico2_w.uf2`

If also installing an LFS filesystem use a similar procedure to copy the appropriate
`filesystem_*.uf2` file.

## Usage Notes

For information BBC BASIC syntax and usage, see the
[BBC BASIC Manual](https://www.bbcbasic.co.uk/bbcwin/manual/).
Note that the description of the IDE in sections 1 and 2 of the documentation is not applicable,
this is not supported by the Pico version.

Major differences between **PicoBB** and other versions
of **BBCSDL** are suumerised [here](https://memotech-bill.github.io/PicoBB/).

## Custom Builds

The `make` command used alone produces the standard builds. Custom builds may be produced
by adding parameters to `make` command.

`make` supports the following options;

* BOARD=... to specify a board other than the default "pico".
* TYPE=... for third party boards to characterise processor and WiFi.
* ADDON=... to specify a file extending or overriding processor board default pin allocations.
  Typically one of vgaboard_sd, vgaboard_serial, vgaboard_cut. The corresponding header file
  must be located in `PicoBB/boards`.
* SUFFIX=... to specify a string to append to the build folder name and the resulting
  buuild files.
* STDIO=... to select the user interface devices. A list from the following, with options
  separated by plus signs:
  * USB for the basic console on USB.
  * UART for the basic console on UART.
  * PICOCALC for the PicoCalc keyboard.
  * BT for the basic console on Bluetooth (Experimental).
  (Examples: STDIO=USB, STDIO=UART, STDIO=USB+UART, STDIO=USB+UART+BT)
* GRAPH=... to select the graphical display device. One of:
  * VGA for output to a VGA display.
  * PICOCALC for output to the PicoCalc LCD
  * WSLCD35 for output to the Waveshare 3.5-inch LCD
* LFS=Y to include storage on Pico flash or LFS=N to exclude it.
* FAT=Y to include storage on SD card or FAT=N to exclude it. To include the SD card
  requires specifying a board which defines pins to use for the SD card.
* SOUND=... to specify sound output. Enabling sound requires specifying a board which
  defines pins to use for sound output.
  * SOUND=N No sound.
  * SOUND=I2S to enable emulation of an SN76489 chip with sound via I2S.
  * SOUND=PWM to enable emulation of an SN76489 chip with sound via PWM.
  * SOUND=SDL to enable enhanced sound similar to other versions of BBCSDL, running on
    the second core of the Pico.
* PRINTER=N to specify no printer or PRINTER=Y to specify a printer attached to the default
  serial port. Only applicable to the GUI build. Requires a board definition with a usable
  serial connection.
* SERIAL_DEV=... To specify serial device support:
  * SERIAL_DEV=0 for no serial devices.
  * SERIAL_DEV=1 for one serial device, the one not used for the console.
  * SERIAL_DEV=2 for two serial devices. This should only be used if the console does not use a UART.
  * SERIAL_DEV=-1 for serial device using the default UART (GUI build only). If PRINTER=Y is
    also selected then output to the printer and to the serial device will be interlaced.
    This serial connection appears as /dev/serial in the file system, and uses the Pico stdio
    interface.
* CYW43=... For Pico W builds (Note: BOARD=pico_w or another board using a Pico W must be specified):
  * CYW43=GPIO for support of Pico W GPIOs only (including onboard LED).
  * CYW43=POLL for network support requiring periodic polling. This option reduces the memory
    available to BASIC.
  * CYW43=BACKGROUND for network support with automatic (background) polling. This option reduces
    the memory available to BASIC.
* NET_HEAP=... Controls allocation of network memory pool.
  * NET_HEAP=0 Use **pico-sdk** default network memory allocation (static memory pool for
    CYW43=BACKGROUND).
  * NET_HEAP=1 Allocate memory pool by moving **HIMEM** down.
* MIN_STACK=Y to use revised code in expression evaluation to minimise stack utilisation. This
  appears to work but has not been as extensively validated as the original BBCSDL code. Use
  MIN_STACK=N to use coding identical to BBCSDL. MIN_STACK=X provides further reduction in
  stack usage at some cost in execution speed (this option is not supported by Richard Russell).
* OS_RAM=<size> to specify how much RAM (in kilobytes) to reserve for low level functions, which
  is unavailable to BASIC. The default builds should default to the correct amount of RAM, but
  if some of the above options are selected it may be necessary to adjust this.
* STACK_CHECK=n Selects different stack checking options:
  * Bit 0 - Check in interpretor loop.
  * Bit 1 - Check in expression evaluator.
  * Bit 2 - Check using a memory barrier. Enables `stack_check()` as the "hard fault" handler
    which will print register values and then return to BASIC with error code 255.

Note: In a non-default build, if bit 2 of the STACK_CHECK option is not set then
`m0FaultDispatch` is optionally linked. The license for this library is free for hobby
and other non-commercial products.  If you wish to create a commercial version of the
program contained here, please add -DFREE to the CMakeLists.txt file.
