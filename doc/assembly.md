# Using 68k machine language with Lox

In this section I show some examples how Lox68k interacts with assembly code. 

## Modifying memory
You can read/write memory with the `peek` and `poke` native functions for byte access or with
the `peekw` and `pokew` library functions for word (16 bit) access and the `peekl` and `pokel`
functions for long word (31 bit) access. 

For example, type 
``` javascript
poke($f0000, %11100110);
```
to set the debug LEDs on the kit.

## Breaking Lox into the Monitor
Type `trap();` into the REPL and you'll be put into the monitor. Now you can 
inspect registers and memory, load additional programs, modify memory, etc. Just be sure you don't
modify the PC nor the SP. Otherwise you won't be able to return to Lox.

## Continuing Lox
Press **PC** and **GO** to return to Lox with the value `nil`. Hopefully you didn't change
the PC and SP registers meanwhile.

## Inspecting Lox data structures
In a freshly started Lox, evaluate
``` javascript
var str = "Inspect me!";
print hex(addr(str));
```
which prints `15922` in release 1.5 (may change in later versions). Now break into the Monitor
by evaluating `trap();` and press **ADDR** and input the address you saw. Now press **HEX** to
dump the memory containing the string object. You'll see
```
00015922:00 01 59 52 09 00 00 0B CF 79 72 3A 49 6E 73 70  ..YR.....yr:Insp
00015932:65 63 74 20 6D 65 21 00                          ect me!.        
```
(Data after the string object has been omitted.) Have a look at `struct ObjString` in file
`object.h` to see what those bytes mean:
  * `00 01 59 52` is a pointer to the next object in the heap.
  * `09` is the type tag (`enum ObjType`), meaning `OBJ_STRING`.
  * `00` is the mark flag for garbage collection.

Upto here the memory layout is the same for all object types. After that, the type-specific
stuff starts.
  * `00 0B` (= 11) is the length of the string.
  * `CF 79 72 3A` is a 32 bit hash of the string
  * `49 6E 73 70 65 63 74 20 6D 65 21 00` are the ASCII characters of the string itself,
    including a terminating zero byte for C.

## Loading machine code to be called from Lox
Do this the usual way before starting Lox. Press the **LOAD** key, press **F3** in the terminal
and input the file you want to upload (see terminal.py for the corresponding file path)
Though Lox uses a lot of RAM, the address range form `$00400` to `$01fff` is reserved for
extensions, where you can load your machine code.


## Static breakpoints for debugging Lox
Three `TRAP #1` [static breakpoints](https://github.com/bayerf42/Monitor/blob/main/doc/monitor_doc.md)
are included and can be activated via **SHIFT** **±TRP1** before starting Lox68k.

 1. `46b0a` at the start of compiler
 2. `4caa6` at the start of the virtual machine
 3. `4e580` after virtual machine has returned

The addresses above are immediately after the breakpoints and only valid for release 1.5 and
certainly will change later. They're only a hint where you are when stepping thru the interpreter
with traps enabled.

When adding dynamic breakpoints, please remember that the code is in ROM and you have to use
**SHIFT** **CONT** instead of **GO** to stop at those breakpoints. OTOH, when reaching 
the third breakpoint, continue with **GO**, so that I/O with the REPL runs at full speed.

Use the listing file `clox_rom.lst` included in the release to associate the
machine code breakpoints with the C source code. 
 
## Sample code: calling machine code to reverse a Lox list in place
In the monitor, press **LOAD**. In the terminal press F3 and input the hex file name `lox_reverse`.
Then, press **ADDR** and input `500`. This is the start address of the `lox_reverse` code. Now press
**SHIFT** **±BRK** to insert a breakpoint at the start of the `lox_reverse` subroutine.

Now start Lox by pressing **ADDR** `44000` **GO**.

Input `print exec($500, [1,2,3]);` into the REPL.

You'll stop at the breakpoint you set before. Now you can inspect anything, or single step thru
the code (only 18 steps until `RTS`), play around.
To continue, press **PC** and **GO** and the REPL will display `[3, 2, 1]`.

## Division by zero 
*Since V1.5 division by zero is handled in Lox itself and no more traps into the Monitor to
be more consistent with other errors.*
 

