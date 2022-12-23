# Native functions in Lox68k

Trailing `?` indicates optional parameter.


| Name        | Parameters             | Returns     | Availability | Description                                                                       |
|-------------|------------------------|-------------|--------------|-----------------------------------------------------------------------------------|
| abs         | real                   | real        | all          | absolute value                                                                    |  
| addr        | val                    | num or nil  | Kit, Emu     | address of an object in heap, nil for immediate values                            |  
| append      | list, val              | -           | all          | appends *val* to end of *list*                                                    |
| asc         | string, index?         | byte        | all          | ASCII code of first or indexed character                                          |  
| atan        | real                   | real        | all          | arc tangent                                                                       |  
| bit_and     | num, num               | num         | all          | bitwise AND of 2 numbers                                                          |
| bit_not     | num                    | num         | all          | bitwise NOT of a number                                                           |
| bit_or      | num, num               | num         | all          | bitwise inclusive OR of 2 numbers                                                 |
| bit_shift   | num, num               | num         | all          | first number shifted left, if second positive, right if negative by second number |
| bit_xor     | num, num               | num         | all          | bitwise exclusive OR of 2 numbers                                                 |
| chr         | byte                   | string      | all          | character with ASCII code *byte*                                                  |
| clock       | -                      | num         | all          | runtime in milliseconds after start. Returns 0 on Kit, since no clock available.  |  
| cos         | real                   | real        | all          | cosine                                                                            |  
| cosh        | real                   | real        | all          | hyperbolic cosine                                                                 |  
| dbg_code    | bool                   | -           | all          | prints byte code after compiling                                                  |  
| dbg_gc      | bool                   | -           | all          | prints garbage collection diagnostics                                             |  
| dbg_stat    | bool                   | -           | all          | print statistics (steps, allocations) after execution                             |  
| dbg_stress  | bool                   | -           | all          | force garbage collection before each allocation, slow!                            |  
| dbg_trace   | bool                   | -           | all          | trace each VM instruction executed, prints stack                                  |  
| dec         | num                    | string      | all          | *num* as decimal string                                                           |
| delete      | list, index            | -           | all          | deletes element at *index* from *list*                                            |
| exec        | addr, val?, val?, val? | val         | Kit, Emu     | executes subroutine at *addr* with upto 3 values on stack, returns value in `D0`  |  
| exp         | real                   | real        | all          | exponential                                                                       |  
| gc          | -                      | num         | all          | forces garbage collection, returns size of allocated memory                       |  
| globals     | -                      | list        | all          | list of all global variable names                                                 |  
| hex         | num                    | string      | all          | *num* as hexadecimal string                                                       |
| index       | val, list, index?      | index or nil| all          | search *val* in *list*, returns index where found, optional search start *index*  |  
| input       | string?                | string      | all          | input string from terminal with optional prompt                                   |
| insert      | list, index, val       | -           | all          | inserts *val* into *list* at position *index*                                     |
| keys        | instance or class      | list        | all          | list of all field names/dictionary keys of an instance or methods in a class      |
| keycode     | -                      | code or nil | Kit          | code of key currently pressed or nil                                              |  
| lcd_clear   | -                      | -           | Kit          | clears LCD                                                                        |  
| lcd_defchar | num, list of 8 bytes   | -           | Kit          | creates a user-defined char with code *num* for LCD with bit pattern from *list*  |  
| lcd_goto    | column, line           | -           | Kit          | set cursor position on LCD                                                        |  
| lcd_puts    | string                 | -           | Kit          | writes *string* to LCD                                                            |  
| log         | real                   | real        | all          | natural logarithm                                                                 |  
| lower       | string                 | string      | all          | lower case of string                                                              |  
| length      | list or string         | num         | all          | length of *list* or *string*                                                      |
| parse_int   | string                 | num or nil  | all          | parses *string* as integer                                                        |
| parse_real  | string                 | real or nil | all          | parses *string* as real                                                           |
| peek        | addr                   | byte        | Kit, Emu     | reads byte from *addr*                                                            |  
| poke        | addr, byte             | -           | Kit, Emu     | writes *byte* to *addr*                                                           |  
| pow         | real, real             | real        | all          | power                                                                             |  
| random      | -                      | num         | all          | a pseudo-random positive number                                                   |
| real        | real                   | real        | all          | convert any number to real                                                        |  
| remove      | instance, val          | bool        | all          | removes a field name/dictionary key from an instance, true if it existed before   |
| seed_rand   | num                    | num         | all          | set seed for random number generator, returns current state                       |
| sin         | real                   | real        | all          | sine                                                                              |  
| sinh        | real                   | real        | all          | hyperbolic sine                                                                   |  
| sleep       | duration               | -           | all          | busy waits for *duration* milliseconds                                            |  
| sound       | rate, duration         | -           | Kit          | plays a sound on speaker, cycle length *rate*, for *duration* milliseconds        |  
| tan         | real                   | real        | all          | tangent                                                                           |  
| tanh        | real                   | real        | all          | hyperbolic tangent                                                                |  
| trap        | -                      | -           | Kit, Emu     | breaks to monitor, now you can inspect Lox internals, can be continued with **GO**|  
| trunc       | real                   | num         | all          | truncate real to integer                                                          |  
| type        | val                    | string      | all          | type (as string) of *val*                                                         |  
| upper       | string                 | string      | all          | upper case of string                                                              |  

