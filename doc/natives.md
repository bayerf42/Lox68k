# Native functions in Lox68k

Native functions are implemented in C and are always available, even when you start Lox without
the [standard library](stdlib.md).

Trailing `?` indicates optional parameter. `num` means any number type, `int` or `real`.

Trailing `?` at return type means that the native may return `nil`.


| Name        | Parameters             | Returns     | Availability | Description                                                                       |
|-------------|------------------------|-------------|--------------|-----------------------------------------------------------------------------------|
| abs         | num                    | num         | all          | absolute value                                                                    |  
| addr        | any                    | int?        | all          | address of an object in heap, nil for immediate values                            |  
| append      | list, any              | nil         | all          | appends *any* to end of *list*                                                    |
| asc         | string, int?           | int         | all          | ASCII code of first character or at index *int*                                   |  
| atan        | num                    | real        | all          | inverse tangent                                                                   |  
| bin         | int                    | string      | all          | *int* as binary string                                                            |
| bit_and     | int, int               | int         | all          | bitwise AND of 2 integers                                                         |
| bit_not     | int                    | int         | all          | bitwise NOT of an integer                                                         |
| bit_or      | int, int               | int         | all          | bitwise inclusive OR of 2 integers                                                |
| bit_shift   | int, int               | int         | all          | first integer shifted left, if second positive, right if negative by second int   |
| bit_xor     | int, int               | int         | all          | bitwise exclusive OR of 2 integers                                                |
| chr         | int                    | string      | all          | character with ASCII code *int*                                                   |
| class_of    | any                    | class?      | all          | class of *any* if it's an instance or bound method, nil otherwise                 |  
| clock       | -                      | int         | all          | runtime in milliseconds after start                                               |  
| cos         | num                    | real        | all          | cosine                                                                            |  
| cosh        | num                    | real        | all          | hyperbolic cosine                                                                 |  
| dbg_call    | any                    | nil         | all/debug    | trace call/return Lox functions/methods, including arguments and return value     |  
| dbg_code    | any                    | nil         | all/debug    | prints byte code after compiling (argument treated as bool, like other dbg funs)  |  
| dbg_gc      | int                    | nil         | all/debug    | prints garbage collection diagnostics according to bit flags *int*, see `memory.h`|  
| dbg_nat     | any                    | nil         | all/debug    | trace calling Lox natives, including arguments and return value                   |  
| dbg_stat    | any                    | nil         | all/debug    | print statistics (time, steps, allocations, GCs) after execution                  |  
| dbg_step    | any                    | nil         | all/debug    | trace each VM instruction executed, prints stack                                  |  
| dec         | num                    | string      | all          | *num* as decimal string                                                           |
| delete      | list, int              | nil         | all          | deletes element at *int* from *list*                                              |
| disasm      | fun, int               | int?        | all/debug    | prints VM code of *fun* at offset *int*, returns next offset or nil at end        |
| equal       | any, any               | bool        | all          | more lenient than ==, numbers compared numerically, iterators for same position   |  
| error       | any                    | *no return* | all          | throws an error with exception *any*. Can be caught with `handle` expression.     |  
| exec        | int, any?, any?, any?  | any         | all          | executes subroutine at address *int* with upto 3 values on stack, return in `D0`  |  
| exp         | num                    | real        | all          | exponential                                                                       |  
| gc          | -                      | int         | all          | forces garbage collection, returns size of allocated memory                       |  
| heap        | int                    | any         | all          | Lox value stored at address *int* in heap                                         |
| hex         | int                    | string      | all          | *int* as hexadecimal string                                                       |
| index       | any, list, int?        | int?        | all          | search *any* in *list*, returns index where found, optional search start *int*    |  
| index       | string, string, int?   | int?        | all          | search 1st *string* in 2nd *string*, returns index, optional search start *int*   |  
| input       | string?                | string      | all          | input string from terminal with optional prompt                                   |
| insert      | list, int, any         | nil         | all          | inserts *any* into *list* at position *int*                                       |
| it_clone    | iterator               | iterator    | all          | clones *iterator*. Both iterators move independently, compare them with equal     |
| join        | list, sep?, beg?, end? | string      | all          | joins all strings in *list*, separated by *sep*, prefix *beg*, suffix *end*       |
| keycode     | -                      | int?        | Kit          | code of key currently pressed or nil                                              |  
| lcd_clear   | -                      | nil         | Kit          | clears LCD                                                                        |  
| lcd_defchar | int, list *of 8 bytes* | nil         | Kit          | creates a user-defined char with code *int* for LCD with bit pattern from *list*  |  
| lcd_goto    | int *col*, int *line*  | nil         | Kit          | set cursor position on LCD                                                        |  
| lcd_puts    | string                 | nil         | Kit          | writes *string* to LCD                                                            |  
| length      | list or string         | int         | all          | length of *list* or *string*                                                      |
| list        | int, any?              | list        | all          | creates a list with *int* elements, initialized to *any* or nil                   |
| log         | num                    | real        | all          | natural logarithm                                                                 |  
| lower       | string                 | string      | all          | lower case of string                                                              |  
| name        | any                    | string?     | all          | name of class, closure, bound, or native, nil for other values                    |  
| next        | iterator               | bool        | all          | advances *iterator* to next entry, returns true when valid                        |  
| parent      | class                  | class?      | all          | retrieve super class of *class* or nil                                            |
| parse_int   | string                 | int?        | all          | parses *string* as integer                                                        |
| parse_real  | string                 | real?       | all          | parses *string* as real                                                           |
| peek        | int                    | int         | all          | reads byte from address *int*                                                     |  
| poke        | int *addr*, int *byte* | nil         | all          | writes *byte* to *addr*                                                           |  
| pow         | num, num               | real        | all          | power                                                                             |  
| random      | -                      | int         | all          | a pseudo-random positive integer                                                  |
| remove      | instance, any          | bool        | all          | removes a field name/dictionary key from an instance, true if it existed before   |
| reverse     | list                   | list        | all          | creates a list initialized by *list* in reverse order                             |
| seed_rand   | int                    | int         | all          | set seed for random number generator, returns current state                       |
| sin         | num                    | real        | all          | sine                                                                              |  
| sinh        | num                    | real        | all          | hyperbolic sine                                                                   |  
| sleep       | int *dur*              | nil         | all          | busy waits for *dur* milliseconds                                                 |  
| slots       | instance               | iterator    | all          | iterator over all slots in *instance*                                             |
| sound       | int *rate*, int *dur*  | nil         | Kit          | plays a sound on speaker, cycle length *rate*, for *dur* milliseconds             |  
| sqrt        | num                    | real        | all          | square root                                                                       |  
| tan         | num                    | real        | all          | tangent                                                                           |  
| tanh        | num                    | real        | all          | hyperbolic tangent                                                                |  
| trap        | -                      | nil         | Kit, Emu     | breaks to monitor, now you can inspect Lox internals, can be continued with **GO**|  
| trunc       | num                    | int         | all          | truncate real to integer                                                          |  
| type        | any                    | string      | all          | type (as string) of *any*                                                         |  
| upper       | string                 | string      | all          | upper case of string                                                              |  
| valid       | iterator               | bool        | all          | does *iterator* address a valid entry?                                            |  

