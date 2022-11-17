# Memory maps for Lox68k

All memory sizes have been rounded and leave some space at the upper limit. However,
the lower limit is fixed, with the exception of the processor stack, which grows from
higher to lower addresses.

All addresses are specified with 5 hex digits, covering the 1 MB address range of the
68008 chip.

## <span id="ROM">Lox68k in ROM (recommended)</span>

This variant allows the maximum amount of RAM to be used by Lox by placing all Lox code
and constants into ROM. Build this version via the project file `clox_rom.prj`, run
`python makerom.py` and burn the image into ROM/Flash and plug it into the Kit.


| Low address   | High address   | Size  | Type | Usage                                |
|---------------|----------------|------:|------|--------------------------------------|
| `$00000`      | `$001ff`       |   ½ k | RAM  | Exception vectors                    |
| `$00200`      | `$003ff`       |   ½ k | RAM  | Monitor variables                    |
| `$00400`      | `$01fff`       |   7 k | RAM  | free                                 |
| `$02000`      | `$1bfff`       | 104 k | RAM  | Lox variables                        |
| `$1c000`      | `$1ffff`       |  16 k | RAM  | processor stack                      |
| `$20000`      | `$3ffff`       | 128 k | -    | unassigned                           |
| `$40000`      | `$43fff`       |  16 k | ROM  | Monitor code and constants           |
| `$44000`      | `$4ffff`       |  48 k | ROM  | Lox code and constants               |
| `$50000`      | `$5ffff`       |  64 k | ROM  | free                                 |
| `$60000`      | `$fffff`       | 640 k | -    | I/O or unassigned                    |


## <span id="RAM">Lox68k in RAM (for experimentation)</span>

This variant puts the entire Lox system into RAM, so you can modify the Lox implementation
without having to burn it into ROM. However, several memory sizes had to be reduced to fit
everything into the available 128 k of RAM. Due to a bug in the IDE68K C compiler, the
standard code layout had to be changed and Lox code starts at `$10000`.

Build this version via the project file `clox.prj` and upload the resulting hex file
`clox.hex` to the Kit (taking some time).

| Low address   | High address   | Size  | Type | Usage                                |
|---------------|----------------|------:|------|--------------------------------------|
| `$00000`      | `$001ff`       |   ½ k | RAM  | Exception vectors                    |
| `$00200`      | `$003ff`       |   ½ k | RAM  | Monitor variables                    |
| `$00400`      | `$0ffff`       |  63 k | RAM  | Lox constants and variables          |
| `$10000`      | `$19fff`       |  40 k | RAM  | Lox code                             |
| `$1a000`      | `$1bfff`       |   8 k | RAM  | free                                 |
| `$1c000`      | `$1ffff`       |  16 k | RAM  | processor stack                      |
| `$20000`      | `$3ffff`       | 128 k | -    | unassigned                           |
| `$40000`      | `$43fff`       |  16 k | ROM  | Monitor code and constants           |
| `$44000`      | `$5ffff`       | 112 k | ROM  | free                                 |
| `$60000`      | `$fffff`       | 640 k | -    | I/O or unassigned                    |

