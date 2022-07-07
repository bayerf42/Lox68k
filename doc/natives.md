# Native functions in Lox68k

Trailing `?` indicates optional parameter.


| Name        | Parameters             | Returns     | Availability | Description                                                                       |
|-------------|------------------------|-------------|--------------|-----------------------------------------------------------------------------------|
| abs         | num                    | num         | all          | absolute value                                                                    |  
| append      | list, item             | -           | all          | appends *item* to end of *list*                                                   |
| asc         | string, index?         | byte        | all          | ASCII code of first or indexed character                                          |  
| bit_and     | num, num               | num         | all          | bitwise AND of 2 numbers                                                          |
| bit_not     | num                    | num         | all          | bitwise NOT of a number                                                           |
| bit_or      | num, num               | num         | all          | bitwise inclusive OR of 2 numbers                                                 |
| bit_shift   | num, num               | num         | all          | first number shifted left, if second positive, right if negative by second number |
| bit_xor     | num, num               | num         | all          | bitwise exclusive OR of 2 numbers                                                 |
| chr         | byte                   | string      | all          | character with ASCII code *byte*                                                  |
| clock       | -                      | num         | all          | runtime in millisecondss after start. Returns 0 on Kit, since no clock available. |  
| dbg_code    | bool                   | -           | all          | prints byte code after compiling                                                  |  
| dbg_gc      | bool                   | -           | all          | prints garbage collection diagnostics                                             |  
| dbg_stat    | bool                   | -           | all          | print statistics (steps, allocations) after execution                             |  
| dbg_stress  | bool                   | -           | all          | force garbage collection before each allocation, slow!                            |  
| dbg_trace   | bool                   | -           | all          | trace each VM instruction executed, prints stack                                  |  
| dec         | num                    | string      | all          | *num* as decimal string                                                           |
| delete      | list, index            | -           | all          | deletes item at *index* from *list*                                               |
| exec        | addr, num?, num?, num? | num         | Kit, Emu     | executes subroutine at *addr* with upto 3 numbers on stack, returns number in `D0`|  
| globals     | -                      | list        | all          | list of all global variable names                                                 |  
| hex         | num                    | string      | all          | *num* as hexadecimal string                                                       |
| index       | item, list, index?     | index or nil| all          | search *item* in *list*, returns index where found, optional search start *index* |  
| input       | string?                | string      | all          | input string from terminal with optional prompt                                   |
| insert      | list, index, item      | -           | all          | inserts *item* into *list* at position *index*                                    |
| int         | string                 | num         | all          | parses *string* as number                                                         |
| keycode     | -                      | code or nil | Kit          | code of key currently pressed or nil                                              |  
| lcd_clear   | -                      | -           | Kit          | clears LCD                                                                        |  
| lcd_defchar | num, list of 8 bytes   | -           | Kit          | creates a user-defined char with code *num* for LCD with bit pattern from *list*  |  
| lcd_goto    | column, line           | -           | Kit          | set cursor position on LCD                                                        |  
| lcd_puts    | string                 | -           | Kit          | writes *string* to LCD                                                            |  
| length      | list or string         | num         | all          | length of *list* or *string*                                                      |
| peek        | addr                   | byte        | Kit, Emu     | reads byte from *addr*                                                            |  
| poke        | addr, byte             | -           | Kit, Emu     | writes *byte* to *addr*                                                           |  
| random      | -                      | num         | all          | a pseudo-random positive number                                                   |
| seed_rand   | num                    | num         | all          | set seed for random number generator, returns current state                       |
| sleep       | duration               | -           | Kit, Emu     | busy waits for *duration* milliseconds                                            |  
| slots       | instance               | list        | all          | list of all field names                                                           |
| sound       | rate, duration         | -           | Kit          | plays a sound on speaker, cycle length *rate*, for *duration* milliseconds        |  
| trap        | -                      | -           | Kit, Emu     | breaks to monitor, now you can inspect Lox internals, can be continued with **GO**|  
| type        | item                   | string      | all          | type (as string) of *item*                                                        |  

