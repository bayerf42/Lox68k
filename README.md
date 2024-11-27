# The Lox language running on the Sirichote 68008 kit

## Synopsis

### What is the Lox language?

It is a high-level interpreted programming language in the spirit of Python, Lua, or Javascript.
Lox is dynamically typed, has garbage collection and supports functional programming idioms with
closures and first-class functions. It also includes a simple class-based object system.
Lox is controlled by a read-eval-print loop (*REPL*).
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
single board computer.

The Lox68K port also contains several language extensions and native functions to access the
Kit peripherals and to integrate machine level coding. I wish I had such a system in the 80ies
when learning computing, but had to use stupid BASIC...

For testing and comparison, I also added a few `#ifdef`s to the source to be able to compile
it as Windows/Linux executables, too.

[Continue reading the docs](doc/lox68k.md)
