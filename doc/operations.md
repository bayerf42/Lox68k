# Lox68k data types, operators, natives, and library functions

## Types

### Any
`= == != addr case const error id name print return string type when`


### Bool
Works for any type, used in a boolean context
 
`! and if for or while`


### Int
`+ - * / \ < <= > >= abs asc bin bit_and bit_not bit_or bit_shift bit_xor chr dec hex max min
parse_int random seed_rand trunc`


### Real
`+ - * / \ < <= > >= abs acos acosh asin asinh atan atanh cos cosh dec deg exp log max min
parse_real pi pow rad sin sinh sqrt tan tanh trunc`


### String
`+ < <= > >= [] [:] asc bin chr dec hex input join length lower match max min parse_int
parse_real split upper`


### List
`+ [] [:] .. append delete filter index insert join length list map reduce reverse split`


### Class
`() < instance Map Object parent Set super`


### Instance
`[] . class_of clone init instance remove slots super this`


### Closure
`() -> complement const disasm id handle`


### Native
`()`


### Bound
`() disasm`


### Iterator
`@ ^ next slots`


## Topics

### Machine-level support
`addr exec heap peek peekl peekw poke pokel pokew trap`

### Input/output
`input keycode print sound`

### System
`clock disasm gc sleep`

### Exceptions
`error handle`

### LCD control
`lcd_clear lcd_defchar lcd_goto lcd_puts`

### Debugging
`dbg_call dbg_code dbg_gc dbg_nat dbg_stat dbg_step disasm`

## Some numbers
* 21 keywords
* 70 native functions
* 62 VM opcodes
* 12 data types
* about 7000 lines source code
* 56 k bytes code size on 68008 kit (48 k without debugging functions)
* Performance
  * 10k ops on 68008 kit
  * 70M ops on Raspberry 4 Linux
  * 300M ops on Intel i7 Laptop Windows 11

