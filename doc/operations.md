# Lox68k operators, natives, and library functions

## Types

### Any
`= == != addr case equal name print return string type when`


### Bool
Works for any type, used in a boolean context
 
`! and if for or while`


### Int
`+ - * / \ < <= > >= abs asc bin bit_and bit_not bit_or bit_shift bit_xor chr dec hex max min
parse_int random seed_rand trunc`


### Real
`+ - * / < <= > >= abs acos acosh asin asinh atan atanh cos cosh dec deg exp fmod log max min
parse_real pi pow rad sin sinh sqrt tan tanh trunc`


### String
`+ < <= > >= [] [:] asc bin chr dec hex input length lower parse_int parse_real upper`


### List
`+ [] [:] .. append delete filter index insert length list map reduce reverse`


### Class
`() < class_name Map Object Set super`


### Instance
`[] . class_of clone init remove slots super this`


### Closure
`() -> complement fun_name`


### Native
`()`


### Bound
`()`


### Iterator
`@ ^ globals it_clone next slots valid`


## Topics

### Machine-level support
`addr exec heap peek peekl peekw poke pokel pokew trap`

### Input/output
`input keycode list_globals print sound`

### System
`clock error gc globals sleep`

### LCD control
`lcd_clear lcd_defchar lcd_goto lcd_puts`

### Debugging
`dbg_call dbg_code dbg_gc dbg_nat dbg_stat dbg_step`

## Some numbers
* 18 keywords
* 68 native functions
* 60 VM opcodes
* 12 data types
* about 6000 lines source code
* 48 k bytes code size on 68008 kit
* Performance
  * 7k ops on 68008 kit
  * 60M ops on Raspberry 4 Linux
  * 230M ops on Intel i7 Laptop Windows 11

