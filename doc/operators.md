# Operators in Lox68k

`num` means any number type, `int` or `real`, integers may be automatically converted to real
when neccessary.

## Unary (prefix) operators
| Op  | Arg      | Result             | Precedence | Description                                           |
|-----|----------|--------------------|------------|-------------------------------------------------------|
| !   | any      | bool               | 8          | logical negation, always bool                         |  
| -   | num      | num                | 8          | additive inverse                                      |  

## Binary (infix) operators
All binary operators associate to the left, with assignment being the only exception.
| Op  | Arg 1    | Arg 2     | Result | Precedence | Description                                           |
|-----|----------|-----------|--------|------------|-------------------------------------------------------|
| *   | num      | num       | num    | 7          | numeric product                                       |  
| /   | int      | int       | int    | 7          | truncated quotient                                    |  
| /   | num      | num       | real   | 7          | real quotient                                         |  
| \   | int      | int       | int    | 7          | remainder of divison                                  |  
| \   | num      | num       | real   | 7          | remainder of divison (like C fmod)                    |  
| +   | num      | num       | num    | 6          | numeric sum                                           |  
| +   | string   | string    | string | 6          | string concatenation                                  |  
| +   | list     | list      | list   | 6          | list concatenation                                    |  
| -   | num      | num       | num    | 6          | numeric difference                                    |  
| <   | num      | num       | bool   | 5          | numerically less than                                 |  
| <=  | num      | num       | bool   | 5          | numerically less than or equal                        |  
| >   | num      | num       | bool   | 5          | numerically greater than                              |  
| >=  | num      | num       | bool   | 5          | numerically greater than or equal                     |  
| <   | string   | string    | bool   | 5          | alphabetically less than                              |  
| <=  | string   | string    | bool   | 5          | alphabetically less than or equal                     |  
| >   | string   | string    | bool   | 5          | alphabetically greater than                           |  
| >=  | string   | string    | bool   | 5          | alphabetically greater than or equal                  | 
| ==  | any      | any       | bool   | 4          | equality/identity                                     |  
| !=  | any      | any       | bool   | 4          | non-equality/non-identity                             |  
| and | any      | any       | any    | 3          | first arg when it's falsey, second arg otherwise      |  
| or  | any      | any       | any    | 2          | first arg when it's truish, second arg otherwise      |  
| =   | place    | any       | any    | 1, right   | assignment to variable or assignable place (see below)|

## Postfix operators
All postfix operators have precedence 9 (highest). Some postfix operators denote assignable places,
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
