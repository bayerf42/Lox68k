# The Lox language running on the Sirichote 68008 kit

## Synopsis

### What is the Lox language?

It is a high-level interpreted programming language in the spirit of Python, Lua, or Javascript.
Lox is dynamically typed, has garbage collection and supports functional programming idioms with
closures and first-class functions. It also includes a simple class-based object system,
featuring single inheritance, constructors and super calls. Lox is controlled by a
read-eval-print loop (*REPL*).
Here is an [overview of the Lox language](https://craftinginterpreters.com/the-lox-language.html).


### What is the Sirichote 68008 Kit?

It is an educational single-board computer using the ancient Motorola 68008 CPU,
designed by Wichit Sirichote, [described here](https://kswichit.net/68008/68008.htm).
It is mainly intended to learn programming the 68k in machine language. It has no operating
system, just a little monitor program and communicates with a terminal (emulator) via RS232.
If you want to buy/build one, contact Wichit via his web site.


### The combination

Lox was designed as a toy language to study the implementation of interpreters, but
due to its small size and simplicity it's very well suited as a scripting language for a
single board computer with very restricted resources.

The Lox68K port also contains several [native functions](natives.md) to access the
Kit peripherals and to integrate machine level coding. I wish I had such a system in the 80ies
when learning computing, but had to use stupid BASIC...

For testing and comparison, I also added a few `#ifdef`s to the source to be able to compile
it as Windows/Linux executables, too.


## Prerequisites

* To build the 68008 version of Lox, you need the
  *IDE68K suite* from https://kswichit.net/68008/ide68k30.zip
* To build the Windows version of Lox, you need the
  *Tiny C compiler* from https://bellard.org/tcc
* To build the Linux version of Lox, you need nothing but standard *gcc*.
* For building the ROM image and running the special Lox terminal emulation,
  you need Python 3, available at https://www.python.org/

If you just want to checkout Lox with the IDE68K emulator, you need nothing else,
just compile it as described below. But if you want to execute it on the Kit itself,
you need the Monitor 4.7 ROM, available either in `rom_image/monitor4.7.bin` or with sources at
https://kswichit.net/68008/Fred/MonitorV4.7.zip
But you should rather combine the Monitor and Lox into a single ROM image, see below.


## Differences to the [original Lox language](https://craftinginterpreters.com/the-lox-language.html)

### Extensions

* A list datatype with according primitives.
* Indexing composite data with `[]` operator, by number (strings and lists) or any value (instances)
* [Slicing syntax](grammar.md) to extract subsequences from strings or lists,
  including negative indices to index from the end of the list or string,
  [like in Python](https://www.w3schools.com/python/python_strings_slicing.asp).
* Several [native functions](natives.md) for type conversion, list handling
  and low-level functions on the 68008 kit.
* The compile-time debugging options are now selectable at runtime via the `dbg_*` native functions.
* Runtime statistics via `dbg_stat()`.
* Hexadecimal literals via the `$` prefix, like `$ff` == `255`.
* Modulo operator `%`
* Interrupting long-running computations by **IRQ** key (or `ctrl-C` in Windows/Linux versions).
* `print` allows list of expression, all printed on one line. With trailing comma,
  no line feed is printed.
* Anonymous functions (*lambdas*) as expressions.
* Functions can have a rest parameter packing all additional arguments into a list.
* Lists can be unpacked as arguments into a function/method call.

### Omissions

The original C implementation of Lox is aimed towards a modern 64 bit architecture
including plenty of RAM and a FPU where the principal `Value` type is
[IEEE-754 double precision floating point](https://en.wikipedia.org/wiki/Double-precision_floating-point_format)
(pretty much like in Lua, Python and Javascript) and other values are embedded as NANs.

For the 68008 port the `Value` type has been shrunk to 32 bit, where numbers
are 31 bit signed integer now, using 1 bit to discriminate from pointers or special values.

Don't forget we're targetting an architecture 40 years old, which runs about 16000 times
slower than contemporary CPUs. Also the IDE68K C compiler has [several bugs](porting.md)
and doesn't support modern *C99*. 


### Lox68k datatypes

  * `nil` the absent/dontcare/missing value
  * `false`, `true` boolean values
  * 31 bit signed integer numbers
  * strings, non-modifiable and interned for quick comparisons
  * lists, modifiable resizable arrays of arbitrary values
  * closures, proper lexically-scoped functions
  * classes containing methods and supporting single inheritance
  * object instances using dictionary as fields, indexable by any value


## The 4 varieties of Lox buildable

All varieties are compiled from the same source files as described in the following sections.

### Lox compiled to a Windows executable.
It actually uses the same memory model and restrictions as the 68008 version and also omits
the 68008 kit hardware features of course and  uses a slightly different I/O and interrupt model.

To build it on Windows, execute
```sh
tcc -owlox.exe *.c -m32
```

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

### Lox compiled to a Linux executable.
The same as the Windows version, but compiled with `GCC` on Linux.

To build it, execute
```sh
gcc -O2 -std=gnu89 -m32 -o llox *.c
```

Be sure to compile it for 32 bit architecture, Lox68k assumes 32 bit `int`, `long` and pointers.


### Lox compiled for the 68008 kit in ROM
It is burnt into ROM (together with the Monitor code)
and can [utilize the entire RAM for data](memorymap.md#ROM) and the kit's hardware like LCD, keyboard,
sound, and terminal communication via the serial port.

To build this version, load project `clox_rom.prj` in *IDE68K* and build it.
A hex file `clox_rom.hex` is generated, which is then combined with the Monitor 4.7
by executing 
```sh
python makerom.py
```
creating `rom_image/mon4.7-lox1.0.bin`, which you burn into EPROM/Flash and plug into the Kit.

To start it, start a terminal emulation, either from *IDE68K*, or preferrably by typing
```sh
python terminal.py
```
press **ADDR**, input address `$44000` and press **GO**. Now you can
send commands to the REPL running at the Kit via the terminal.

To interrupt long-running computations, press the **IRQ** key bringing you back to the input
prompt. Be sure to put the interrupt source switch to *IRQ*, not to *10ms TICK*.

You can also run this version in the *IDE68K* emulator, but you have to set its start address
`$44000` manually into the PC register.

### Lox compiled for the 68008 kit in RAM
It is uploaded into RAM (which takes quite some time)
and has some [tighter memory limits](memorymap.md#RAM) to fit everything into the
available 128k RAM, but you don't have to burn a ROM image after every modification
while testing.

To build this version, load project `clox.prj` and build it. A hex file `clox.hex` is generated,
which can be uploaded to the Kit, or executed with the *IDE68K* emulator.

The start address is `$00400` as usual, you interact with it the same way as the ROM version.


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
**IRQ** key on the Kit.

The entire terminal session is written to `transcript.log`. 

In lines 17ff in `terminal.py` you can adapt the file paths and serial device name for
your needs.

