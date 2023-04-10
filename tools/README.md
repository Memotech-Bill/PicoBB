# Tools for working with Pico BBC BASIC

## 99-pico.rules

If PicoBB is repeatedly restarted while cconnected to a Raspberry Pi
(or possibly any other Linux machine) its USB connection can take on
any one of a number of different device names, such as `/dev/ttyACM0`,
`/dev/ttyACM1`, `/dev/ttyACM2`, etc.

This file contains a UDEV rule which will create a link `/dev/pico`
which always refers to the current USB connection.

Copy the file into `/etc/udev/rules.d`.

## yupload.bas - Automatically upload and run a program

This is a BBC BASIC program to run on a host computer to upload and
test another program on console PicoBB, connected by either USB or
UART.

`yupload.bas` may be run using either BBCSDL or console BBC BASIC on
the host machine. It performs the following actions:

* Prompts for the name of the program to upload.
* Prompts for the device name to use to connect to the Pico if unknown.
  The answer is remembered and not requested again.
* Resets the Pico if it is connected via USB.
* Sends CR and ESC characters to try and get a BASIC prompt.
* If a prompt is obtained, attempts to upload the program into the root
  storage on the Pico.
* Requests the Pico to send BBC BASIC VDU sequences rather than VT100
  escape sequences
* Starts the uploaded program running.
* Sends keystrokes from the host computer to the Pico, and displays the
  output from the Pico.

Once the Pico program is running, `yupload.bas` disables break on <ESC>
key, instead it will send an <ESC> key press to the Pico. To exit
`yupload.bas` use the key sequence <Ctrl+A><Ctrl+X> (the same sequence
as is used to exit `picocom`). Typing <Ctrl+A> twice sends a single
<Ctrl+A> to the Pico. <Ctrl+A> followed by any other character will
send both characters to the Pico. Note that when a <Ctrl+A> is typed,
no Pico output will be displayed until the next character is typed.
