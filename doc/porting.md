# Issues on porting CLOX for the IDE68K C compiler

* Original C dialect CLOX is written in is C99

* IDE68K C compiler is mostly C89, but supporting several newer features. However, several
  compiler bugs had to be circumvented...


## Compiler bugs

  * variable declarations in nested blocks (`case`!) generate wrong code without notification
  * Compiler crashes sometimes on syntax errors, e.g. missing closing parens.
  * Structures return by value: Compiler generates wrong code with struct sizes less than 12 bytes, an `FMOVE` is generated???
    Solution is making structs at least 12 bytes, so the compiler generates a `DBRA` loop
    to copy 3 longwords (or more).
  * When using smaller field sizes in `Obj` and derived structures, the compiler generates wrong
    code when embedding the entire `Obj` structure as first member in derived structures like
    `ObjFunction`. When `Obj` was 8 bytes long, everything worked, but when reducing it to
    6 bytes (to save heap, `type` and `marked` only need 1 byte), the compiler used 4 bytes as structure
    size, leading to totally wrong offsets in derived structures. Fixed by manually expanding
    `Obj`s fields in each derived structure.
  * Some macros were resolved incorrectly, e.g. `READ_CONSTANT` in `vm.c`. Had to expand
    manually.
  * Wrong word-size absolute addressing
    * When a static (non exported) function in address range `$8000` - `$ffff` is called, the
      assembler generates absolute `.W` calls, which is wrong, since those calls sign-extend
      the address and thus call `$ffff xxxx`.
    * So the entire code has to be moved outside the 68k 'zero page' to force the assembler to
      use `.L` calls.
    * This is only a problem when code is in RAM, in ROM the code is above `$0004 0000` anyway.
    * Quick fix for code in RAM: shuffle sections, move `data` and `bss` segments to low memory,
      start code at `$0001 0000`.



## Compiler incompatibilities

  * variable declaration inside of a block, all variables have to be declared
   _at the start of a function_
  * variable declarations after a statement cause error
  * an unsigned 16-bit value stays unsigned when converting to 32 bit, an explicit cast is
    required
  * Cannot use structure initialization by field name or array index (compiler precedence table)
  * `inline` not supported


## C Library and start-up bugs

  * `memcpy` uses wrong code copying long-word-wise, not caring for alignment, had to write
    `fix_memcpy` which copies byte-wise.
  * `realloc` function is broken, had to implement own version, luckily all allocations are routed thru `reallocate` in
    `memory.c`. 
  * `malloc()` and `free()` seem buggy, totally replaced by `nano_malloc.c`
  * Avoid (non-zero) initialized DATA when building a ROM image
    * Loading a HEX file includes DATA sections and puts them in RAM, so all is ok.
    * But there's no mechanism in the C startup, which would create a DATA section in RAM from ROM.
    * BSS sections in ROM-able code are ok, these are zeroed by the C startup.
  * `printf("%s", str)` inserts arbitrary spaces, when `str` is longer than 127 characters.


## Reducing memory size, other hardware restrictions

  * Replaced gray stack in `VM` by static array.
  * Several `int` fields in structures have been changed to smaller int types to reduce memory
    requirements.
  * Decreased size of value stack in `VM`.
  * The `compiler` struct was very big and caused stack overflows since it was pushed onto
    the C stack for every nested function declaration.
    By reducing the allowed locals (64) and upvalues (32) the compiler struct on the
    stack has been reduced to allow a sensible nesting of functions within the total
    C stack area of 16k.
  * No floating point.
    * The 68008 has no built-in FP arithmetic. All `Number`s are 31-bit signed integers, the
      remaining bit discriminates numbers from `Obj` pointers and specials, see `value.h`.
    * Perhaps sometimes I find/implement a soft FP library, then NaN boxing could be used again,
      just like in the original CLOX implementation, but with single floats (32 bit) instead of
      doubles (64 bit).

  
## Board limitations

  * There's no OS, no file system, no permanent storage (other than ROM), no multithreading/tasking,
    no asynchronous I/O, just one interrupt (used directly to interrupt the VM)
  * There are only routines to read/write a single byte over the serial interface at 9600 baud,
    so all I/O (including loading source code) has to be routed over those. The C library does
    this automatically, so functions like `printf` and `gets` (with simple line editing) can be
    used.

# Porting to other 68k-based SBCs
  Should be quite easy when using *IDE68K*. 

  * Remove the Sirichote Kit specific native functions like `keycode`, `lcd_*`, and `sound`
    from `native.c`
  * Provide native functions for your board's peripherals
  * Change the memory layout (section start addresses) in `cstart_lox_rom.asm` for your system
  * Provide your board's routines for `__putch`, `__getch`, and `__exit` in `cstart_lox_rom.asm`
  * Change `handleInterrupts` in `native.c` for your board to allow interrupting infinite loops
  * Change the `CHECK_STACKOVERFLOW` macro in `common.h` if you want to check it in critical functions.
  * Porting to 68020-based boards should be no problem, just set the processor option in
    *IDE68K* accordingly. No assumptions about exception stack format or privileged SR access
    are made. Also no extra information is stored in the upper 8 bits of pointers.

  

 