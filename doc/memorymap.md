# Memory map for Lox68k

All memory sizes have been rounded and leave some space at the upper limit. However,
the lower limit is fixed, with the exception of the processor stack, which grows from
higher to lower addresses.

All addresses are specified with 5 hex digits, covering the 1 MB address range of the
68008 chip.


| Low address   | High address   | Size  | Type | Usage                                |
|---------------|----------------|------:|------|--------------------------------------|
| `$00000`      | `$001ff`       |   ½ k | RAM  | Exception vectors                    |
| `$00200`      | `$003ff`       |   ½ k | RAM  | Monitor variables                    |
| `$00400`      | `$01fff`       |   7 k | RAM  | free                                 |
| `$02000`      | `$1bfff`       | 104 k | RAM  | Lox variables including 64 kB heap   |
| `$1c000`      | `$1ffff`       |  16 k | RAM  | processor stack                      |
| `$20000`      | `$3ffff`       | 128 k |      |                                      |
| `$40000`      | `$43fff`       |  16 k | ROM  | Monitor code                         |
| `$44000`      | `$51fff`       |  56 k | ROM  | Lox code [debug]                     |
| `$52000`      | `$5dfff`       |  48 k | ROM  | Lox code [no debug]                  |
| `$5e000`      | `$5efff`       |   4 k | ROM  | Lox standard library source code     |
| `$5f000`      | `$5ffff`       |   4 k | ROM  | Motorola FFP library                 |
| `$60000`      | `$fffff`       | 640 k |      | some addresses used for I/O          |


