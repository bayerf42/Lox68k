# The Lox language running on the Sirichote 68008 kit

## Synopsis

### What is the Lox language?

It is a high-level interpreted programming language in the spirit of Python, Lua, or Javascript.
Lox is dynamically typed, has garbage collection and supports functional programming idioms with
closures and first-class functions. It also includes a simple class-based object system.
Lox is controlled by a read-eval-print loop (*REPL*).
Here is an [overview of the Lox language](https://craftinginterpreters.com/the-lox-language.html).


### What is the Sirichote 68008 Kit?

It is an [educational single-board computer](https://kswichit.net/68008/68008.htm),
designed by Wichit Sirichote, using the ancient Motorola 68008 CPU.
It is mainly intended to learn programming the 68k in machine language. It has no operating
system, just a little monitor program and communicates with a terminal (emulator) via RS232.
If you want to buy/build one, contact Wichit via his web site.


### The combination

Lox was designed as a toy language to study the implementation of interpreters, but
due to its small size and simplicity it's very well suited as a scripting language for a
single board computer with very restricted resources.

The Lox68K port contains [many extensions](extensions.md) to make it a useful programming
system including access to the Kit's peripherals and integrating machine level coding.
I wish I had such a system in the 80ies when learning computing, but had to use stupid BASIC...

For testing and comparison, Lox68k compiles to Windows/Linux executables, too.


## Prerequisites

* To build the 68008 version of Lox, you need the
  *IDE68K suite* from https://kswichit.net/68008/ide68k30.zip
* To build the Windows version of Lox, you need the
  *Tiny C compiler* from https://bellard.org/tcc or any other suitable 32 bit C compiler
* To build the Linux version of Lox, you need nothing but standard *gcc*.
* For building the ROM image and running the special Lox terminal emulation,
  you need Python 3, available at https://www.python.org/ and the `pyserial` and `bincopy`
  packages, installable with `pip`.

If you just want to checkout Lox with the IDE68K emulator, you need nothing else,
just compile it as described below. But if you want to execute it on the Kit itself,
you need the [Monitor ROM](https://github.com/bayerf42/Monitor)
and the [Motorola FFP library](https://github.com/bayerf42/MotoFFP).

If you don't want to build the ROM yourself, the final ROM image `mon_ffp_lox.bin` is included
in the release. Just burn it into an EPROM or flash chip.

## Differences to the [original Lox language](https://craftinginterpreters.com/the-lox-language.html)

### Summary of [Extensions](extensions.md), [Syntax](grammar.md)

* A [list datatype](extensions.md#list) with according primitives
* [Indexing](extensions.md#index) and [slicing](extensions.md#slice)
  composite data with `[]` [operator](operators.md)
* 72 [native functions](natives.md) for type conversion, collections, math
  and [low-level functions](assembly.md) on the 68008 Kit
* [Debugging options](extensions.md#debug) selectable at runtime
* [Hexadecimal](extensions.md#hex) and [binary](extensions.md#bin) literals
* [Interrupting](extensions.md#interrupt) evaluation
* [`print`](extensions.md#print) syntax extension
* [Anonymous functions (*lambdas*)](extensions.md#lambda)
* [Variadic](extensions.md#variadic) arguments for functions
* Instance slots [iterators](extensions.md#iterator)
* Function body can be abbreviated by [`-> expr`](extensions.md#arrow) 
* [Standard library](stdlib.md) written in Lox68k
* [`case`](extensions.md#case) for multiway branch
* [`break`](extensions.md#break) to leave loops early
* Some [class extensions](extensions.md#class)
* [`handle`](extensions.md#exception) for exception handling
* [`if` expression](extensions.md#if_expr), in addition to `if` statement 
* expressions at [top-level](extensions.md#toplevel) 
* [`dynvar`](extensions.md#dynvar) allows re-binding of global variables

### Omissions

The original C implementation of Lox is aimed towards a modern 64 bit architecture
including plenty of RAM and a FPU where the principal `Value` type is
[IEEE-754 double precision floating point](https://en.wikipedia.org/wiki/Double-precision_floating-point_format)
(pretty much like in Lua, Python and Javascript) and other values are embedded as NANs.

For the 68008 port the `Value` type has been shrunk to 32 bit, where numbers
are 31 bit signed integers, using 1 bit to discriminate from pointers or special values.

All memory sizes have been shrunk to 16 bit to save memory, so the maximum heap size is 64k, which
is adequate for the Sirichote Kit.

**New:** Floating point numbers have been re-introduced as heap-based objects. They can be freely
mixed with integers in calculations. Also a standard set of transcendental functions has been
added. 

Don't forget we're targetting an architecture 40 years old, which runs about 20000 times
slower than contemporary CPUs. Also the IDE68K C compiler has [several bugs](porting.md)
and doesn't support modern *C99*. 


### Lox68k datatypes

  * `nil` the absent/dontcare/undefined value
  * `false`, `true` boolean values
  * 31 bit signed integer numbers
  * real numbers, as Motorola 32 bit Fast Floating Point on Kit, or IEEE-754 doubles on other hardware
  * strings, non-modifiable and interned for quick comparisons
  * lists, modifiable resizable arrays of arbitrary values
  * closures, proper lexically-scoped functions
  * natives, library functions written in C or assembly
  * classes containing methods and supporting single inheritance
  * object instances using a hashtable for fields, but also indexable by any value
  * bound methods, combining an instance with a method
  * iterators for traversing hashtables

### Datatypes and operations
  [Listed here](operations.md)


## <a id="varieties"></a>The 4 varieties of Lox buildable

All varieties are compiled from the same source files as described in the following sections.

Depending on the `LOX_DBG` symbol, two versions of Lox are compiled:
  1. Debug version including various debug and disassembly natives
  2. A smaller and (about 10%) faster version without those natives


### Lox running on the actual 68008 kit
The executable is burnt into ROM (together with the Monitor code and the FFP library)
and can [utilize the entire RAM for data](memorymap.md) and the kit's hardware like LCD, keyboard,
sound, and terminal communication via the serial port.

To build this version, load project `clox.prj` in *IDE68K* and build it.
A hex file `clox.hex` is generated. Do the same with project `clox_dbg.prj` generating
hex file `clox_dbg.hex`.

Combine both hex files with the Monitor by executing 
```sh
python makerom.py
```
creating `../roms/mon_ffp_lox.bin`, which you burn into EPROM/Flash and plug into the Kit.

To start it, start a terminal emulation, either from *IDE68K*, or preferrably by typing
```sh
python terminal.py
```
press **ADDR**, input one of the start address `$44000`, `$44008`, `$52000`, or `$52008`
[(what's the difference)](stdlib.md) and press **GO**.
Now you can send commands to the REPL running at the Kit via the terminal.

To interrupt long-running computations, press the **REP** key bringing you back to the input
prompt. Be sure to put the interrupt source switch to *10ms TICK* to have an accurate timer.
In versions before 1.4, the **IRQ** key was used for this purpose.


### Lox running in the *IDE68K* emulator
You can also run this version in the *IDE68K* emulator, but Kit's I/O (LCD, sound, keyboard)
are not available. To use the Floating point and standard library, you must have built the
ROM file as described above before starting the emulator.
Just press the *RUN* icon and input Lox code into the console.

To load a file into the REPL, input
```
&filepath
```
at the REPL prompt, just like Windows and Linux variants.

Please note that the visual simulator of *IDE68K* may crash easily when outputting text
at a high rate, the textual simulator is more stable.

You can also configure the Visual Simulator's peripherals to behave quite like the actual Kit:

  * **Switches**: Set to `00080000` Now bit 6 controls the running flag, set it to `1` before
    starting an evaluation and set it to `0` to interrupt it.
  * **LEDs**: Set to `000F0000` to simulate the Kit's LEDs.
  * **Timer**: Set to `00000268` to count 10 Hz ticks, which is 10 times slower
    than the Kit's 100 Hz ticks, so all timings measured (via `clock()` or `dbg_stat()`)
    have to be multiplied by 10.  


### Lox compiled to a Windows executable.
It actually uses the same memory model and restrictions as the 68008 version and also omits
the 68008 kit hardware features of course and uses a slightly different I/O and interrupt model.

To build it on Windows, execute
```sh
build
```
to generate both `wlox.exe` and `wlox_dbg.exe`.

To start Lox interactively (aka. the REPL), execute
```sh
wlox
```

To start Lox evaluating files and terminating, execute
```sh
wlox file1 file2 ...
```

To start the REPL after loading files, exceute
```sh
wlox file1 file2 ... -
```

`ctrl-C` while waiting for input terminates Lox, `ctrl-C` during computation brings
you back to input, leaving environment intact.

You can also load a file from the REPL by inputting
```
&filepath
```
at the REPL prompt.


### Lox compiled to a Linux executable.
Works exactly as the Windows version, but compiled with `GCC` on Linux.

To build it, execute
```sh
build
```

Be sure to compile it for 32 bit architecture, Lox68k assumes 32 bit `int`, `long` and pointers.

## The terminal emulator
You can interact with Lox68K running on the Kit with any terminal program, e.g., the one included
in IDE68K, or with Putty, etc. However, since you want to upload Lox source code and
68008 hex files to the Kit, I recommend using `terminal.py` instead, which provides those two
features. It supports both serial protocols for uploading HEX files and the (different)
protocol `gets()` uses on the Kit for inputting source code to the Lox REPL.

Start it with
```sh
python terminal.py
```
Now everything you type is sent to the Kit and every response
from the kit is displayed. Additionally, you can upload Lox source code by pressing **F2** and
inputting the file name. Or you can upload Motorola HEX files by pressing the **LOAD** button
on the kit, pressing **F3** in the terminal and input a hex file name.

Since Lox reads the entire input (file) into RAM, the maximum file size is limited by the
input buffer size, which is 16 kB for the ROM version of Lox68k.

When inputting Lox code, **ENTER** sends your input to the Lox REPL and interprets it. When
you want to input a line feed without starting interpretation, press **Ctrl-ENTER** instead.

To exit the terminal, press **Ctrl-C**, but to interrupt a running computation, press the
**REP** key on the Kit.

The entire terminal session is written to `transcript.log`. 

In lines 17ff in `terminal.py` you can adapt the file paths and serial device name for
your needs.

