# Using 68k machine language with Lox

## Starting Lox68k

## Modifying memory
Type 
```javascript
poke($f0000, %11100110);
```
to set the debug LEDs on the kit.


## Loading machine code to be called from Lox

## Breaking Lox into the Monitor

## Continuing Lox

## Static breakpoints for debugging Lox
Three `TRAP #1` [static breakpoints](https://github.com/bayerf42/Monitor/blob/main/doc/monitor_doc.md)
are included and can be activated via **SHIFT** **±TRP1** before starting Lox68k.

 1. at the start of compiler
 2. at the start of the virtual machine
 3. after virtual machine has returned
 
When adding dynamic breakpoints, please remember that the code is in ROM and you have to use
**SHIFT** **CONT** instead of **GO** to stop at those breakpoints. OTOH, when stopping after
VM has returned, continue with **GO**, so that I/O with the REPL runs at full speed.

## Sample code: calling machine code to reverse a Lox list

## Division by zero (real)
In Lox, evaluate the expression `print 1.0/0.0;`. The program traps into the Monitor and
displays `Err div0` on the LEDs. Now inspect the divisor in data register D6 displaying `00000000`
which is 0.0 in FFP format, and the dividend in data register D7 displaying `80000041` which
is 1.0 in FFP format (highest bit = most significant bit of mantissa, `$41` is biased exponent).

You can fix this condition by specifying an alternative divisor and retrying the computation, this
behaviour is provided for in the Motorola FFP library!

To do so, edit register D6 by pressing **SHIFT** **D6** **EDIT** and input `E0000043`
(= 7.0 in FFP) and press **GO**. The division is retried with dividend 7.0 and displays
the correct result `0.14285715` in the REPL.

## Division by zero (int)
Very similar to the previous case, but you have to manually adjust the PC to continue:

Evaluate the expression `print 1000000/0;` Again you are trapped into the Monitor displaying
`Err div0`.

Here pressing **PC** displays the instruction *after* the trapping divison. So press **-**
twice to see the trapping instruction `DIVU D1,D0`.

You see that the divisor 0 is in register D0 and (part of) the dividend is in register D1.
To fix the divisor, edit register D1 by pressing **SHIFT** **D1** **EDIT**
and input `7`. Now press **PC** (you are back *after* the trapping instruction) and press **-**
twice again to restart the trapping `DIVU` instruction and press **GO**.

Now the evaluation is continued and you'll see the result of the integer divison `1000000/7`
which is `142857` in the REPL.
 

