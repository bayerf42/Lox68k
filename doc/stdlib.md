# Lox68k standard library

Trailing `?` indicates optional parameter. `num` means any number type, `int` or `real`.
`val` means a value of any type.

## Functions

| Name         | Parameters            | Returns     | Description                                                                       |
|--------------|-----------------------|-------------|--------------|--------------------------------------------------------------------|
| acos         | num                   | real        | inverse cosine                                                                    |  
| acosh        | num                   | real        | inverse hyperbolic cosine                                                         |  
| asin         | num                   | real        | inverse sine                                                                      |  
| asinh        | num                   | real        | inverse hyperbolic sine                                                           |  
| atanh        | num                   | real        | inverse hyperbolic tangent                                                        |  
| class_name   | class                 | string      | name of *class*                                                                   |  
| class_of     | instance              | class       | class of *instance*                                                               |  
| complement   | fun                   | fun         | boolean complement of *fun*                                                       |  
| filter       | fun, list             | list        | filter all items of *list* which satisfy predicate *fun*                          |  
| fmod         | num, num              | real        | floating point modulo                                                             |  
| fun_name     | fun                   | string      | name of closure *fun*                                                             |  
| list_globals | -                     | nil         | prints all global variables                                                       |  
| map          | fun, list             | list        | apply *fun* to every item in *list*, return list of results.                      |  
| max          | num, num              | num         | maximum of arguments                                                              |  
| max          | string, string        | string      | maximum of arguments                                                              |  
| min          | num, num              | num         | minimum of arguments                                                              |  
| min          | string, string        | string      | minimum of arguments                                                              |  
| native_addr  | native                | int         | address of *native*                                                               |  
| peekl        | int                   | int         | reads 31 bit long from address *int*                                              |  
| peekw        | int                   | int         | reads 16 bit word from address *int*                                              |  
| pi           |                       | real        | variable containing pi = 3.1415926535897932384626433                              |  
| radians      | num                   | real        | convert from degrees to radians                                                   |  
| reduce       | fun, list             | any         | combine all items of *list* with operator *fun* from the left                     |  
| string       | any                   | string      | convert *any* to a string, using *string* method for objects.                     |  


## Classes

| Name        | Methods   | Parameters    | Returns     | Description                                                                       |
|-------------|-----------|---------------|-------------|--------------|--------------------------------------------------------------------|
| Object      |           | -             |             | base class for every Lox class, defines string() and clone() methods.             |
|             | clone     | -             | class       | method used to shallow-copy an object                                             |  
|             | string    | -             | string      | method used to convert an object to a string                                      |  
| Map         |           | any*          |             | a map containing key/value pairs, making the internal hash table explicit         |
| Set         |           | any*          |             | a set implemented as a hash table                                                 |
|             | intersect | Set           | Set         | intersect *Set* with receiver, modifying it                                       |  
|             | subtract  | Set           | Set         | subtract *Set* from receiver, modifying it                                        |  
|             | union     | Set           | Set         | unite *Set* with receiver, modifying it                                           |  
