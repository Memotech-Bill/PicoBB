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

There are currently four standard builds:

### Pico Console version: bbcbasic_console_pico_w in folder console/pico

* Will run on a Pico or Pico W.
* Intended to be able to use the Pico as a microcontroller programmed in BBC Basic.
* User interaction (if any) is via a console interface over USB or serial.
* Includes program storage in Pico flash.
* High quality sound (as per BCSDL) is available as PWM from two GPIO pins.
* Support for networking over the built in WiFi on a Pico W.
* Has 228KB of RAM available, shared between BASIC and the machine stack.

### Pico2 Console version: bbcbasic_console_pico2_w in folder console/pico

* Will run on a Pico2 or Pico2 W.
* Intended to be able to use the Pico2 as a microcontroller programmed in BBC Basic.
* User interaction (if any) is via a console interface over USB or serial.
* Includes program storage in Pico flash.
* High quality sound (as per BCSDL) is available as PWM from two GPIO pins.
* Support for networking over the built in WiFi on a Pico2 W.
* Has 494KB of RAM available, shared between BASIC and the machine stack.

### Pico GUI version: bbcbasic_gui_pico_w_vgaboard_sd in folder bin/pico

* Will run on a Pico or Pico W.
* Uses the Pico as a computer programmable in BBC Basic with input by an attached USB keyboard,
  and display on an attached VGA monitor.
* The GUI version has been designed to run on a Pico attached to a VGA demonstration board as per
  chapter 3 of
  [Hardware design with RP2040](https://datasheets.raspberrypi.org/rp2040/hardware-design-with-rp2040.pdf)
  or the commercial version
  [Pimoroni Pico VGA Demo Base](https://shop.pimoroni.com/products/pimoroni-pico-vga-demo-base).
* Includes program and data storage either in Pico flash and SD card on the VGA demo board.
* Support for networking over the built in WiFi on a Pico W.
* Sound is low quality (as per BBC Micro) available via the PWM output.
* Has 164KB of RAM available, shared between BASIC and the machine stack.


### Pico2 GUI version: bbcbasic_gui_pico2_w_vgaboard_sd in folder bin/pico

* Will run on a Pico2 or Pico2 W.
* Uses the Pico as a computer programmable in BBC Basic with input by an attached USB keyboard,
  and display on an attached VGA monitor.
* The GUI version has been designed to run on a Pico attached to a VGA demonstration board as per
  chapter 3 of
  [Hardware design with RP2040](https://datasheets.raspberrypi.org/rp2040/hardware-design-with-rp2040.pdf)
  or the commercial version
  [Pimoroni Pico VGA Demo Base](https://shop.pimoroni.com/products/pimoroni-pico-vga-demo-base).
* Includes program and data storage either in Pico flash and SD card on the VGA demo board.
* Support for networking over the built in WiFi on a Pico2 W.
* Sound is low quality (as per BBC Micro) available via the PWM output.
* Has 420KB of RAM available, shared between BASIC and the machine stack.

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
Building the GUI versions requires [pico-extras](https://github.com/raspberrypi/pico-extras)
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

### GUI Versions

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

If, in adddition to attaching the 3x2 header, the tracks on the under-side of the board,
joining the pads GP20 to SD01 and GP21 to SD02 are cut, then it is possible to use both
the UART and the SD card, but the SD card can only be used in 1-bit or SPI modes. This
configuration is described by the vgaboard_cut configuration file.

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

### LCD Build

One possible custom build supports the Waveshare
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
        make BOARD=<pico_type> ADDON=wslcd35 GRAPH=wslcd35 SOUND=N FAT=Y

Where <pico_type> is replaced by the type of Pico being used.
        
## Installation

To install, plug a Pico device into the USB port while holding the boot button and then copy the
required file onto the device using a command such as:

     cp -v bbcbasic_console_pico_w.uf2 /media/pi/RPI-RP2

or:

     picotool install -f bbcbasic_console_pico2_w.uf2

## Usage Notes

For information BBC BASIC syntax and usage, see the
[BBC BASIC Manual](https://www.bbcbasic.co.uk/bbcwin/manual/).
Note that the description of the IDE in sections 1 and 2 of the documentation is not applicable,
this is not supported by the Pico version. Major differences between **PicoBB** and other versions
of **BBCSDL** are suumerised [here](https://memotech-bill.github.io/PicoBB/).

### Connecting to the Console versions (from Raspberry Pi)

If using USB the Pico device may appear as any of `/dev/ttyACMn`, where n is a small
integer. To avid this, the [tools folder](tree/master/tools) contains a udev rule
which creates a link `/dev/pico` to the current connection. With this rule in place,
connect using:

      picocom /dev/pico

Or if using Raspberry Pi serial port, connect as

      picocom -b 115200 /dev/ttyS0

to use the interpreter. If using a serial connection tap the Enter key one or two times
to initiate the connection. The onboard LED will flash while awaiting connection.

Note that you may use minicom as well, however, minicom will not display color correctly
or resize the terminal window for the different MODE settings in BBC Basic.

If connected to the Pico via USB then the Pico can be restarted by changing the USB Baud rate
to special values:

* 1200 - Resets the Pico to BOOTSEL mode, allowing you to load a new UF2 file.
* 2400 - Restarts to BBC BASIC, killing any BASIC program previously running.

### Connecting to the GUI versions

To use (once the Pico has been programmed), assuming a Pico on a VGA Demo board:

* Power should be applied to the micro-USB socket on the demo board.
* Display output is via the VGA connector.
* The keyboard should be connected to the USB socket on the Pico itself.
A USB to micro-USB adaptor will be required. The Pico is somewhat selective
as to which keyboards work, cheap keyboards may be better.
* An SD or SDHC card may be used, It should be formatted as FAT.
* If required an amplified speaker or amplified headphones should be connected
  to the socket on the VGA demo board. If using PWM sound (the default) then use
  the PWM socket. If using I2S sound then use the socket labelled DAC.

### File system

Files are loaded from and saved to the Pico Flash memory. The standard BBC Basic commands may
be used to navigate the folder structure.

If SD card support has been included then the contents of the SD card appear under the /sdcard
folder. A SD or SDHC card may be used and it should be FAT formatted.

### RP2040 Thumb Assembler

The Pico builds include an assembler for the ARM v6M instruction set as supported by the RP2040.
By default the assembler uses "Unified" syntax. However including the following pseudo-op

   syntax d

enables the following extensions to the allowed syntax:

* If an opcode takes three arguments and the first and second are the same, then the first may be omitted.
* If the 's' suffix (indicating affects status flags) is omitted from the opcode, but the parameters are
  only valid for a status affecting instruction, then that is assumed.

The extensions may be disabled again by specifying:

    syntax u

### RP2350 Thumb Assembler

The Pico2 builds include an assembler for most of the ARM v8M instructions supported by the RP2350.
This assembler only supports "Unified" syntax.

### Sound

All sound implementations use the BBC BASIC SOUND and ENVELOPE commands. *STEREO and *VOICE
are also implemented for high quality (SDL) sound.

For the console builds it will be necessary to attach low-pass filters to the sound output
pins (pins 32 & 34, GPIOs 27 & 28) and then connect the output from these to an amplifier.

For the GUI builds, the VGA demo board includes the necessary post-processing, and an amplifier
(or high impedance headphones) can be connected directly to the output sockets.

### Serial Input and Output

Depending upon the build used, serial devices may appear as as single /dev/uart
or the pair /dev/uart0 and /dev/uart1. A serial port may be opened using a command of the form:

   port% = OPENUP("/dev/uart.baud=9600 parity=N data=8 stop=1 tx=0 rx=1 cts=2 rts=3")

Any of the pin numbers (tx, rx, cts, rts) may be omitted in which case that pin will not
be connected. This enables transmit only, or receive only connections, and connections
without flow control. The pin numbers selected must be valid Pico pin numbers for the
relevent UART and function.

If not specified, the following defaults are assumed: parity=N data=8 stop=1.

The baud rate must be specified.

If keyword=value format is used then the parameters may be given in any order. Alternately
the keyword and equals sign may be omitted in which case the parameters must be in the order
shown above.

Note: Contrary to the BBC Basic documentation commas must not be used between the parameters.

If the BBC Basic user interface is connected via USB, then either serial interface may be used.
If the user interface is connected via a serial connection, then do not open that interface
(/dev/uart0 for a bare Pico, /dev/uart1 for a Pico on VGA Demo board).

### Pico SDK

Many of the functions from the Pico C SDK may be accessed using the BBC BASIC command SYS.
This can also access many of the C routines in the interpreter or the C runtime library.
For a list of the supported functions see the build/sympico.h file in the folder for
the version being used.

Note that the ARM processor in the Pico requires that integer values are aligned on word
(four byte) addresses. However, in general BBC BASIC does not in general align its variables.
This is not a problem when passing values into the SDK, but if addresses are being passed
in order to access or return data then these must be correctly aligned. Failing to do so
will result in a "Hard Fault".

The following variables in BBC BASIC are correctly aligned:

* The static integer variables A% to Z%.
* The first variable in a structure. The alignment of subsequent variables will depend
  upon what preceeds them.
* LOCAL arrays.

### Network

The WiFi network is accessed using a Pico specific version of the BBC BASIC "socklib"
library. See the BBC BASIC [manual](https://www.bbcbasic.co.uk/bbcwin/manual/bbcwing.html#socklib)
for details. This library is included in the filesystem images for the network builds.

The first time the library is used you will be prompted for the WiFi details (SSID, password
and two letter country code). These will then be stored in the file "wifi.cfg" for subsequent
use. To change the access point used, delete this file.

The filesystem images also include four example programs:

* **client.bbc** and **server.bbc** - These are a pair of simple two-way chat programs.
  They may be used to exchange messages with the corresponding program running on BBC BASIC
  on another machine on the network.
* **lanchat.bbc** - A simple program to exchange messages with multiple users on a LAN.
* **tftp_server.bbc** - A file transfer program. This may be used to copy text or binary
  files to and from the Pico. A suitable client program may be installed on a Raspberry Pi
  using the command `sudo apt install tftp`.

It is generally not practical to directly use the LWIP network routines provided by the Pico
C SDK as these rely heavily on callbacks into user supplied C (or assembler) routines. Therefore
some higher level routines have been written in C to provide the functions needed to implement
"socklib". If required, these routines may be accessed directly using SYS. They are documented
in the file "include/network.h".

### File Transfer

In order to transfer files files to and from the Pico console versions of BBC BASIC, the XModem,
YModem and ZModem protocols have been implemented. These are accessed by six new **star** commands:

`*xupload <filename>` - Receive an uploaded file using XModem protocol. The name of the file must
be specified, as it is not sent by the upload program. Note that the length of the file will be
padded out to the next multiple of 128 bytes.

`*yupload [<filename>|<dirname/>]` - Receive an uploaded file using YModem protocol. The filename
is optional, but if given will override the name sent by the upload program. If a directory name
(ending in '/') is given, the uploaded file will be placed in that directory. The exact file
length will be preserved.

`*zupload  [<filename>|<dirname/>]` - As for `*yupload` but using ZModem protocol.

`*xdownload <filename>` - Download a file using XModem protocol. It will be necessary to specify
a filename at the receiving end. Note that the length of the file will be padded out to the next
multiple of 128 bytes.

`*ydownload <filename>` - Download a file using YModem protocol. The exact file length is transferred
and the name of the file is supplied to the download program.

`*zdownload <filename>` - As for `*ydownload` but using ZModem protocol.

Enter one of the above commands on the Pico, then on the terminal perform whatever action is
required to start the upload or receive the download. For `picocom` that is <ctrl+A><ctrl+S>
to send a file or <ctrl+A><ctrl+R> to receive a file. `picocom` uses ZModem by default.

For YModem or ZModem uploads it is not strictly necessary to first enter the **star**
command on the Pico, starting of an upload will be automatically detected. However,
doing so will avoid a long delay before a YModem upload starts.

Note: The Modem protocols are old, and not particularly well specified. There may well be
differences in how different programs implement them. The Pico implementation as been tested
using `picocom` with `sx`, `sb` or `sz` (depending upon protocol) for upload, and `rx`, `rb`
or `rz` for download.

TODO: Add support for uploads and downloads on serial ports other than the console connection.

Alternately, some builds may allow file transfer via the network or an SD card.

### System Identification

As per BBC BASIC standard, the low byte (bits 0-7) of the system variable **@platform%** will
have the value 6 for any of the Pico implementations.

Bits 8-11 of this variable will have one of these values:

* 0 - No Pico W support (LED on Pico W unaccesable).
* 1 - Pico W GPIO support only (LED on Pico W accessible).
* 2 - Pico W network support using polling.
* 3 - Pico W network support using background interrupts.

And bits 12-15 will have one of:

* 0 - RP2040 processor.
* 2 - RP2350 processor.

The same information can be obtained using SYS to call the C function `is_pico_w`.

Note: The standard builds will return either 2 or 4 on a Pico W.

The Pico build also supplies a new (read only) system variable **@picocfg&()** which
provides more details of the specific configuration:

* **@picocfg&(0)** - User interface:
  * Bit 0 - Console on USB
  * Bit 1 - Console on UART
  * Bit 2 - GUI user interface
* **@picocfg&(1)** - File systems:
  * Bit 0 - On Pico flash memory (LFS filesystem)
  * Bit 1 - On SD card (FAT filesystem)
  * Bit 2 - Device filesystem (currently only UARTs)
* **@picocfg&(2)** - Sound implementation
  * 0 - No sound
  * 1 - Low quality sound on I2S
  * 2 - Low quality sound using PWM
  * 3 - High quality sound using PWM
* **@picocfg&(3)** - Number of serial devices
  * 0xFF - GUI build, /dev/serial, used by VDU printer commands
* **@picocfg&(4)** - Chip support:
  * Bits 0-3:
    * 0 - No CYW43 support (LED on CYW43 unaccesable).
    * 1 - CYW43 GPIO support only (LED on CYW43 accessible).
    * 2 - CYW43 network support using polling.
    * 3 - CYW43 network support using background interrupts.
  * Bits 4-7:
    * 0 - RP2040 (two M0+ cores).
    * 2 - RP2350 (two M33 cores).

This may be extended in the future if more capabilities are added.

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

To use the feature use the `*output` command to specify the display device

* `*output 0`  - Output to serial console
* `*output 14` - Output to VGA display
* `*output 15` - Output to both serial console and VGA display

Note that since BBC BASIC uses VDU commands to draw any graphics, the VGA display
must be selected as an output device before any graphics can be drawn. The serial
console will ignore the graphics VDU commands.

### GUI version

#### Video Modes

The implementation currently supports 16 video modes:

Mode | Colours |   Text  | Graphics  | Letterbox
-----|---------|---------|-----------|----------
   0 |     2   | 80 x 32 | 640 x 256 |     Y
   1 |     4   | 40 x 32 | 320 x 256 |     Y
   2 |    16   | 20 x 32 | 160 x 256 |     Y
   3 |     2   | 80 x 25 | 640 x 225 |
   4 |     2   | 40 x 32 | 320 x 256 |     Y
   5 |     4   | 20 x 32 | 160 x 256 |     Y
   6 |     2   | 40 x 25 | 320 x 225 |
   7 |     8   | 40 x 25 | Teletext  |
   8 |     2   | 80 x 30 | 640 x 480 |
   9 |     4   | 40 x 30 | 320 x 480 |
  10 |    16   | 20 x 30 | 160 x 480 |
  11 |     2   | 80 x 25 | 640 x 450 |
  12 |     2   | 40 x 30 | 320 x 480 |
  13 |     4   | 20 x 30 | 160 x 480 |
  14 |     2   | 40 x 25 | 320 x 450 |
  15 |    16   | 40 x 30 | 320 x 240 |

Modes 0-7 reproduce those from the BBC Micro. Modes 0-2, 4 & 5 only have 256 rows of pixels
which are displayed in the centre of the monitor so may appear squashed.

Modes 3 & 6 can also display graphics.

Except for Mode 7, colours 8-15 are high intensity rather than flashing.

The generally most useful modes are mode 8, which is a monochrome display with the highest resolution,
and mode 15, which is a 16 colour mode with square pixels. The default mode on startup is mode 8.

#### Screen Refresh

By default the commands "*refresh [off|on]" do nothing. However one of two different implementations
may be enabled.

"*refresh buffer" enables double buffering of the display. In this mode turning refresh off hides any
screen changes until a "*refresh" or "*refresh on" command. For low resolution modes 4-7 or 12-14
both buffers fit within the statically allocated video storage. However for the higher resolution modes
a second buffer is allocated above HIMEM and above any INSTALLed libraries. It will probably be
necessary to lower HIMEM to make sufficient room for this second buffer.

"*refresh queue" enables an alternate mode in which, while refreshing is turned off, all VDU
requests are stored in a queue and then processed as quickly as possible when refreshing is enabled.
The VDU queue is stored above HIMEM and any INSTALLed libraries. However the size of the queue is
typically smaller than a second frame buffer. The size of the queue may be specified by a decimal
number following "queue".

"*refresh none" disables "refresh [off|on]".

#### User defined characters

Command **VDU 23,...** may be used to define user character shapes. The following points should be
noted:

* Character codes are divided into blocks of 32 characters.
* The first user defined character in a block allocates memory for all the characters in the block.
* The first character block overwrites szCmdLine, which has minimal utility for the Pico.
* PAGE has to be raised (by 256 bytes per block) if more than one block of user defined characters
  is required.

#### Serial Input and Output

The default GUI builds on the VGA demo board have no free external connections. With non default
builds, either disabling the SD card or modifying the board (removing links or cutting tracks)
it is possibe to to enable one serial connection.

There are two ways of using this:

* The VDU driver uses the serial port for printer output.
* The serial port is available as /dev/serial.

The serial port may be opened for input or output as:

   port% = OPENUP ("/dev/serial.")

It defaults to the standard 115200 baud, no parity, 8 data bits, 1 stop bit. Other formats may
be specified by using a statement of the form:

   port% = OPENUP("/dev/serial.baud=9600 parity=N data=8 stop=1")

If any of the parameters are omitted then the default values will be used.

If keyword=value format is used then the parameters may be given in any order. Alternately
the keyword and equals sign may be omitted in which case the parameters must be in the order
shown above.

Note: Contrary to the BBC Basic documentation commas must not be used between the parameters.

#### Missing features & Qwerks

The Pico implementation is missing features compared to the BBCSDL implementation
on full operating systems. The limitations include:

* High resolution text (VDU 5) is implemented, but it does not scroll, instead wraps
  around back to the top of the screen. New text overlays any existing rather than
  page or scroll mode.
* PLOT modes 168-191 & 208+ are not implemented.
* Loading and saving bitmapped images is not implemented.
* There is no double-buffering of the display. Any updates are visible immediately.

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
  * BT for the basic console on Bluetooth (Experimental).
  (Examples: STDIO=USB, STDIO=UART, STDIO=USB+UART, STDIO=USB+UART+BT)
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
  * SERIAL_DEV=2 for two serial devices. This should only be used if the console is USB only.
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
* GRAPH=Y (Experimental) Add VGA output to console build.
* OS_RAM=<size> to specify how much RAM (in kilobytes) to reserve for low level functions, which
  is unavailable to BASIC. The default builds should default to the correct amount of RAM, but
  if some of the above options are selected it may be necessary to adjust this.

Note: the Makefile runs Python scripts to automaticly generate C files "sympico.h" and "pico_stub.c"
needed to implement the SYS interface to the Pico SDK functions before running CMake. Therefore
directly running CMake to build the software is not straightforward.

The options supported by the CMake script are similar to those for `make`:

* -DPICO_BOARD=... to specify a board other than the default "pico".
* -DADDON=... to specify a file extending or overriding processor board default pin allocations.
  Typically one of vgaboard_sd, vgaboard_serial, vgaboard_cut. The corresponding header file
  must be located in `PicoBB/boards`.
* -DSTDIO=... to select the user interface:
  * -DSTDIO=PICO for the GUI version with input via USB keyboard and output via VGA monitor.
  * -DSTDIO=... to select a list from the following, with options separated by plus signs:
    * USB for the basic console on USB.
    * UART for the basic console on UART.
    * BT for the basic console on Bluetooth (Experimental).
    (Examples: -DSTDIO=USB, -DSTDIO=UART, -DSTDIO=USB+UART, -DSTDIO=USB+UART+BT)
* -DLFS=Y to include storage on Pico flash or -DLFS=N to exclude it.
* -DFAT=Y to include storage on SD card or -DFAT=N to exclude it.
* -DSOUND=I2S to enable sound via I2S. The pins used for the sound output are specified by
  PICO_AUDIO_I2S_DATA_PIN and PICO_AUDIO_I2S_CLOCK_PIN_BASE in the selected board definition file.
* -DSOUND=PWM to enable sound via PWM. The pins used for the sound output are specified by
  PICO_AUDIO_PWM_L_PIN and PICO_AUDIO_PWM_R_PIN in the selected board definition file. The
  output on the two pins is nominally identical but generated by different PWM channels.
  If PICO_AUDIO_PWM_R_PIN is not specified, then the output is on a single pin given by
  PICO_AUDIO_PWM_L_PIN. If PICO_AUDIO_PWM_L_PIN is not specified, then the output is on
  pin 2. Filtering and probably amplification of the output will be required.
* -DSOUND=SDL to enable SDL style sound using the second core. This option is incompatible
  with -DSTDIO=PICO which uses the second core for video.
* -DPRINTER=N to specify no printer or -DPRINTER=Y to specify a printer attached to the default
  serial port. Only applicable to the GUI build. Requires a board definition with a usable
  serial connection.
* -DSERIAL_DEV=... To specify serial device support:
  * -DSERIAL_DEV=0 for no serial devices.
  * -DSERIAL_DEV=1 for one serial device, the one not used for the console.
  * -DSERIAL_DEV=2 for two serial devices. This should only be used if the console is USB only.
  * -DSERIAL_DEV=-1 for serial device using the default UART (GUI build only). If PRINTER=Y is
    also selected then output to the printer and to the serial device will be interlaced.
    This serial connection appears as /dev/serial in the file system, and uses the Pico stdio
    interface.
* -DSTACK_CHECK=n Selects different stack checking options:
  * Bit 0 - Check in interpretor loop.
  * Bit 1 - Check in expression evaluator.
  * Bit 2 - Check using a memory barrier. Enables `stack_check()` as the "hard fault" handler
    which will print register values and then return to BASIC with error code 255.
* -DMIN_STACK=Y to use revised code in expression evaluation to minimise stack utilisation. This
  appears to work but has not been as extensively validated as the original BBCSDL code. Use
  -DMIN_STACK=N to use coding identical to BBCSDL. -DMIN_STACK=X provides further reduction in
  stack usage at some cost in execution speed (this option is not suppoerted by Richard Russell).
* -DCYW43=... For Pico W builds (Note: A board using a Pico W must be specified):
  * -DCYW43=GPIO for support of Pico W GPIOs only (including onboard LED).
  * -DCYW43=POLL for network support requiring periodic polling. This option reduces the memory
    available to BASIC.
  * -DCYW43=BACKGROUND for network support with automatic (background) polling. This option reduces
    the memory available to BASIC.
* -DGRAPH=Y (Experimental) Add VGA output to console build.
* -DOS_RAM=<size> to specify how much RAM (in kilobytes) to reserve for low level functions, which
  is unavailable to BASIC. The default builds should default to the correct amount of RAM, but
  if some of the above options are selected it may be necessary to adjust this.
* Other cmake options if required.

Note: In a non-default build, if bit 2 of the STACK_CHECK option is not set then
`m0FaultDispatch` is optionally linked. The license for this library is free for hobby
and other non-commercial products.  If you wish to create a commercial version of the
program contained here, please add -DFREE to the CMakeLists.txt file.
