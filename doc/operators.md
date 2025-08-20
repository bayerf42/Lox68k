# Operators in Lox68k

`num` means any number type, `int` or `real`, integers may be automatically converted to real
when neccessary.

## Binary (infix) operators
All binary operators associate to the left, with assignment being the only exception.
| Op  | Arg 1    | Arg 2     | Result | Precedence | Description                                           |
|-----|----------|-----------|--------|------------|-------------------------------------------------------|
| *   | num      | num       | num    | 6          | numeric product                                       |  
| /   | int      | int       | int    | 6          | truncated quotient                                    |  
| /   | num      | num       | real   | 6          | real quotient                                         |  
| \   | int      | int       | int    | 6          | remainder of divison                                  |  
| \   | num      | num       | real   | 6          | remainder of divison (like C fmod)                    |  
| +   | num      | num       | num    | 5          | numeric sum                                           |  
| +   | string   | string    | string | 5          | string concatenation                                  |  
| +   | list     | list      | list   | 5          | list concatenation                                    |  
| -   | num      | num       | num    | 5          | numeric difference                                    |  
| <   | num      | num       | bool   | 4          | numerically less than                                 |  
| <=  | num      | num       | bool   | 4          | numerically less than or equal                        |  
| >   | num      | num       | bool   | 4          | numerically greater than                              |  
| >=  | num      | num       | bool   | 4          | numerically greater than or equal                     |  
| <   | string   | string    | bool   | 4          | alphabetically less than                              |  
| <=  | string   | string    | bool   | 4          | alphabetically less than or equal                     |  
| >   | string   | string    | bool   | 4          | alphabetically greater than                           |  
| >=  | string   | string    | bool   | 4          | alphabetically greater than or equal                  | 
| ==  | any      | any       | bool   | 4          | equality/identity                                     |  
| !=  | any      | any       | bool   | 4          | non-equality/non-identity                             |  
| and | any      | any       | any    | 3          | first arg when it's falsey, second arg otherwise      |  
| or  | any      | any       | any    | 2          | first arg when it's truish, second arg otherwise      |  
| =   | place    | any       | any    | 1, right   | assignment to variable or assignable place (see below)|

## Unary (prefix) operators
All prefix operators have precedence 7 (second highest) and associate to the right.
| Op  | Arg      | Result             | Description                                           |
|-----|----------|--------------------|-------------------------------------------------------|
| !   | any      | bool               | logical negation, always bool                         |  
| -   | num      | num                | additive inverse                                      |  

## Postfix operators
Postfix operators are actually binary operators where the extent of the second operand(s) is uniquely
determined without further precedence parsing.

All postfix operators have precedence 8 (highest). Some postfix operators denote assignable places,
which is partly determined at compile-time and partly at run-time.

| Op  | Arg 1    | Rest args | Result | Assignable | Description                                           |
|-----|----------|-----------|--------|------------|-------------------------------------------------------|
| ()  | callable | any*      | any    | no         | function/method/constructor call                      |  
| []  | string   | int       | string | no         | string index, result is string of single character    |  
| []  | list     | int       | any    | yes        | list index                                            |  
| []  | instance | any       | any    | yes        | property of instance, no class access                 |  
| [:] | string   | int?, int?| string | no         | substring (slice)                                     |  
| [:] | list     | int?, int?| list   | no         | sublist (slice)                                       |  
| .   | instance | name      | any    | yes        | named property of instance, may access class          |  
| @   | iterator | -         | any    | no         | key of iterator                                       |  
| ^   | iterator | -         | any    | yes        | value of iterator                                     |  
