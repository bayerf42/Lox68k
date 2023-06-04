# Native functions in Lox68k

Native functions are implemented in C and are always available, even when you start Lox without
the [standard library](stdlib.md).

Trailing `?` indicates optional parameter. `num` means any number type, `int` or `real`.
`val` means a value of any type.


| Name        | Parameters             | Returns     | Availability | Description                                                                       |
|-------------|------------------------|-------------|--------------|-----------------------------------------------------------------------------------|
| abs         | num                    | num         | all          | absolute value                                                                    |  
| addr        | val                    | int or nil  | all          | address of an object in heap, nil for immediate values                            |  
| append      | list, val              | nil         | all          | appends *val* to end of *list*                                                    |
| asc         | string, int?           | int         | all          | ASCII code of first character or at index *int*                                   |  
| atan        | num                    | real        | all          | inverse tangent                                                                   |  
| bin         | int                    | string      | all          | *int* as binary string                                                            |
| bit_and     | int, int               | int         | all          | bitwise AND of 2 integers                                                         |
| bit_not     | int                    | int         | all          | bitwise NOT of a integer                                                          |
| bit_or      | int, int               | int         | all          | bitwise inclusive OR of 2 integers                                                |
| bit_shift   | int, int               | int         | all          | first integer shifted left, if second positive, right if negative by second int   |
| bit_xor     | int, int               | int         | all          | bitwise exclusive OR of 2 integer                                                 |
| chr         | int                    | string      | all          | character with ASCII code *int*                                                   |
| clock       | -                      | int         | all          | runtime in milliseconds after start. Returns 0 on Kit, since no clock available.  |  
| cos         | num                    | real        | all          | cosine                                                                            |  
| cosh        | num                    | real        | all          | hyperbolic cosine                                                                 |  
| dbg_code    | bool                   | bool        | all          | prints byte code after compiling                                                  |  
| dbg_gc      | int                    | int         | all          | prints garbage collection diagnostics according to bit flags *int*, see `memory.h`|  
| dbg_stat    | bool                   | bool        | all          | print statistics (steps, allocations) after execution                             |  
| dbg_trace   | bool                   | bool        | all          | trace each VM instruction executed, prints stack                                  |  
| dec         | num                    | string      | all          | *num* as decimal string                                                           |
| delete      | list, int              | nil         | all          | deletes element at *int* from *list*                                              |
| error       | string                 | *no return* | all          | Aborts program execution with error message *string*                              |  
| exec        | int, val?, val?, val?  | val         | all          | executes subroutine at address *int* with upto 3 values on stack, return in `D0`  |  
| exp         | num                    | real        | all          | exponential                                                                       |  
| gc          | -                      | int         | all          | forces garbage collection, returns size of allocated memory                       |  
| globals     | -                      | iterator    | all          | iterator over all global variables                                                |  
| heap        | int                    | any         | all          | Lox value stored at address *int* in heap                                         |
| hex         | int                    | string      | all          | *int* as hexadecimal string                                                       |
| index       | val, list, int?        | int or nil  | all          | search *val* in *list*, returns index where found, optional search start *int*    |  
| input       | string?                | string      | all          | input string from terminal with optional prompt                                   |
| insert      | list, int, val         | nil         | all          | inserts *val* into *list* at position *int*                                       |
| it_clone    | iterator               | iterator    | all          | clones *iterator*. Both iterators move independently                              |
| it_same     | iterator, iterator     | bool        | all          | Tests if both *iterator*s point to the same entry.                                |
| keycode     | -                      | int or nil  | Kit          | code of key currently pressed or nil                                              |  
| lcd_clear   | -                      | nil         | Kit          | clears LCD                                                                        |  
| lcd_defchar | int, list *of 8 bytes* | nil         | Kit          | creates a user-defined char with code *int* for LCD with bit pattern from *list*  |  
| lcd_goto    | int *col*, int *line*  | nil         | Kit          | set cursor position on LCD                                                        |  
| lcd_puts    | string                 | nil         | Kit          | writes *string* to LCD                                                            |  
| length      | list or string         | int         | all          | length of *list* or *string*                                                      |
| list        | int, val?              | list        | all          | creates a list with *int* elements, initialized to *val* or nil                   |
| log         | num                    | real        | all          | natural logarithm                                                                 |  
| lower       | string                 | string      | all          | lower case of string                                                              |  
| next        | iterator               | bool        | all          | advances *iterator* to next entry, returns true when valid                        |  
| parse_int   | string                 | int or nil  | all          | parses *string* as integer                                                        |
| parse_real  | string                 | real or nil | all          | parses *string* as real                                                           |
| peek        | int                    | int         | all          | reads byte from address *int*                                                     |  
| poke        | int *addr*, int *byte* | nil         | all          | writes *byte* to *addr*                                                           |  
| pow         | num, num               | real        | all          | power                                                                             |  
| random      | -                      | int         | all          | a pseudo-random positive integer                                                  |
| remove      | instance, val          | bool        | all          | removes a field name/dictionary key from an instance, true if it existed before   |
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
| type        | val                    | string      | all          | type (as string) of *val*                                                         |  
| upper       | string                 | string      | all          | upper case of string                                                              |  
| valid       | iterator               | bool        | all          | does *iterator* address a valid entry?                                            |  

