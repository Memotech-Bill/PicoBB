# LFS and UF2 Utilities

## mklfsimage

Creates or adds to a binary file containing a LittleFS file system.

Usage: `mklfsimage [options] <directories>...`

where the options are:

**-h** - Display a usage summary and exit.

**-o <filename>** - Specifies the filename of the LFS image. The file
is created if it does not exist or added to if already an LFS image.

**-s <size>[K|M]** - Specifies the size of the filesystem. The size may
be given in bytes, kilo-bytes (1024 bytes) if followed by a **K**, or
mega-bytes if followed by an **M**.

**-p <dir>** - Specifies the name of the LFS directory to receive the
inserted files and folders.

**<directories>...** - Directory name (or names) containing files or
folders to be added to the LFS image.

## uf2conv

Converts a file to UF2 format for loading onto a Pico.

Usage: `uf2conv [options] <source_file> <uf2_file> [<address>]`

where the options are:

**-d <device_type>** - One of the following options to select the UF2
family ID:

* **ABSOLUTE** - Write to a fixed location in flash.
* **RP2040** - Code or data intended for an RP2040 (this is the only
ID recognised by the RP2040 chip).
* **DATA** - Arbitrary data.
* **RP2350_ARM_S** - Bootable secure ARM code for the RP2350.
* **RP2350** - Abbreviation for **RP2350_ARM_S**
* **RP2350_RISCV** - Bootable RISCV code for the RP2350.
* **RP2350_ARM_NS** - ARM non-secure mode code for the RP2350

**-f <family-id>** - Specifies the family ID in numeric form in decimal
(beginning `1`-`9`), hex (beginning `0x`) or octal (beginning `0`).

**-m <filename>** - Read the family ID from the specified UF2 file and
use it for the created file.

**<source_file>** - Name of the file to convert to UF2 format.

**<uf2_file>** - Name of the UF2 file to create.

**<address>** - Start address in flash to load the UF2 data.The address may
be given in bytes, kilo-bytes (1024 bytes) if followed by a **K**, or
mega-bytes if followed by an **M**.

## uf2merge

Combile two or more UF2 files into a single file.

Usage: `uf2merge [options] <uf2_file> <uf2_file>...`

where the options are:

**-h** - Display a usage summary and exit.

**-f** - Set the family ID of all the merged sections to the ID of the first
input file.

**-i** - Preserve family ID of different input files. If neither `-f` nor `-i`
are specified, then mismatched family IDs in the input files will result in an
error message and the files not merged.

**-e** - Create a single continuous image in the output file, with any gaps between
the input files filled with a pad byte. Useful for erasing any existing data on the
device that may occupy these gaps.

**-p <pad>** - Specify the pad byte to use to fill any space not defined in the
input files. All sections except the last must be padded to a 4KB boundary, and
if `-e` is specified all gaps are filled with the pad byte. The default pad is 0xFF.

**<uf2_file>** = Names of the UF2 files to merge.

## pico_examples.py

Generates a LittleFS image of BBC BASIC libraries, examples and test programs for
the Pico version of BBC BASIC. Only recreates the output file if it is older than
one of the selected input files.

Usage: `pico_examples.py [options] <config_file>`

Where the options are:

**-v**, **--version** - Display a version number and exit.

**-b <build>**, **--build <build>** - PicoBB build type to select files to include.

**-d <device>**, **--device <device>** - Device type to select files to include.

**-o <filename>**, **--output <filename>** - Name of the file to receive the LittleFF
image.

**-s <size>[K|M]**, **--size <size>[K|M]** - Size of the LittleFS image. `K` at the
end for size in kilo-bytes or `M` at the end for size in mega-bytes.

**-t <dir>**, **--tree <dir>** - Path to a temporary folder to collect all the files
prior to copying into the LFS image.

**<config_file>** - Name of configuration file selecting the files to include.

### Configuration file format

Lines beginning with a hash (#) are comments and the remainder of the line is ignored.
Blank lines are also ignored.

The file is divided into sections. Each section begins with a header line of the form:

````
[<device>, <build>, <dir>]
````

where:

**<device>** - A device name to match that specified on the command line, or `all` to
match any device.

**<build>** - A build name to match that specified on the command line, or `all` to
match any build.

**<lib>** - Directory in the LittleFS image to receive the files

The header is followed by a list of filenames (which may include wildcards) to be
copied into the LittleFS image if the <device> and <build> options in the header
line are matched by the command line options.
