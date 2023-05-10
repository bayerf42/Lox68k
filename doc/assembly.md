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

## Division by zero (int)

## Division by zero (real)

