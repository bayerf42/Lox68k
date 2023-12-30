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
which prints `1667e` in release 1.4 (may change in later versions). Now break into the Monitor
by evaluating `trap();` and press **ADDR** and input the address you saw. Now press **HEX** to
dump the memory containing the string object. You'll see
```
0001667e:00 01 66 BE 09 00 00 0B CF 79 72 3A 49 6E 73 70  ..f......yr:Insp
0001668e:65 63 74 20 6D 65 21 00                          ect me!.        
```
(Data after the string object has been omitted.) Have a look at `struct ObjString` in file
`object.h` to see what those bytes mean:
  * `00 01 66 BE` is a pointer to the next object in the heap.
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

 1. `4687c` at the start of compiler
 2. `4bf38` at the start of the virtual machine
 3. `4d9a8` after virtual machine has returned

The addresses above are immediately after the breakpoints and only valid for release 1.4 and
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

## Division by zero (real)
In the REPL, evaluate the expression `print 1.0/0.0;`. Lox traps into the Monitor and
displays `Err div0` on the LEDs. Now inspect the divisor in data register D6 displaying `00000000`
which is 0.0 in FFP format, and the dividend in data register D7 displaying `80000041` which
is 1.0 in FFP format (highest bit = most significant bit of mantissa, `$41` is biased exponent).

You can fix this condition by specifying an alternative divisor and retrying the computation, this
behaviour is provided for in the Motorola FFP library!

To do so, edit register D6 by pressing **SHIFT** **D6** **EDIT** and input `E0000043`
(= 7.0 in FFP) and press **GO**. The division is retried with divisor 7.0 and displays
the correct result `0.14285715` in the REPL.

## Division by zero (int)
Very similar to the previous case, but you have to manually adjust the PC to continue:

Evaluate the expression `print 1000000/0;` Again you are trapped into the Monitor displaying
`Err div0`.

Here pressing **PC** displays the instruction *after* the trapping divison. So press **-**
twice to see the trapping instruction `DIVU D1,D0`.

You see that the divisor 0 is in register D1 and (part of) the dividend is in register D0.
To fix the divisor, edit register D1 by pressing **SHIFT** **D1** **EDIT**
and input `7`. Now press **PC** (you'll be moved back *after* the trapping instruction) and press **-**
twice again to go to the trapping `DIVU` instruction and press **GO**.

Now the evaluation is continued and you'll see the result of the integer divison `1000000/7`
which is `142857` in the REPL.
 

