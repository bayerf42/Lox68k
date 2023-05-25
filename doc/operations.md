# Lox68k operators, natives, and library functions

## Types

### Any
`= == != addr print return string type`


### Bool
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
`[] . class_of clone init remove slots this`


### Closure
`() -> complement fun_name`


### Native
`() native_addr`


### Bound
`()`


### Iterator
`@ ^ globals it_clone it_same next slots valid`


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
`dbg_code dbg_gc dbg_stat dbg_trace`