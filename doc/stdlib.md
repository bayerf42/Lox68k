# Lox68k standard library

The Lox standard library contains several useful functions and classes which are written
in Lox itself. There are 2 slightly different versions (due to memory alignment and byte order)
for 68k and Intel/ARM architectures, files `lox/stdlib_68k.lox` and `lox/stdlib.lox`.

You can start Lox68k with or without the standard library:

* On the Kit start at `$44000` for Lox *with* the standard library, or at `$44008` for Lox
  *without* the standard library. The standard library is bundled automatically when
  building the ROM image.

* On the 68k Simulator the standard library is not available, you have to use start address
  `$44008` always.

* With the Windows or Linux versions, you have to add the standard library explicitly to the
  command line, e.g.
  ```sh
  wlox lox/stdlib.lox lox/mycode.lox -
  ```

## Functions

| Name         | Parameters            | Returns     | Description                                                                       |
|--------------|-----------------------|-------------|-----------------------------------------------------------------------------------|
| acos         | num                   | real        | inverse cosine                                                                    |  
| acosh        | num                   | real        | inverse hyperbolic cosine                                                         |  
| asin         | num                   | real        | inverse sine                                                                      |  
| asinh        | num                   | real        | inverse hyperbolic sine                                                           |  
| atanh        | num                   | real        | inverse hyperbolic tangent                                                        |  
| class_of     | any                   | class?      | class of *any* if it's an instance, nil otherwise                                 |  
| complement   | fun                   | fun         | boolean complement of *fun*                                                       |  
| deg          | num                   | real        | convert from radians to degrees                                                   |  
| filter       | fun, list             | list        | filter all items of *list* which satisfy predicate *fun*                          |  
| fmod         | num, num              | real        | floating point modulo                                                             |  
| list_globals | -                     | nil         | prints all global variables                                                       |  
| map          | fun, list             | list        | apply *fun* to every item in *list*, return list of results.                      |  
| max          | num, num              | num         | maximum of arguments                                                              |  
| max          | string, string        | string      | maximum of arguments                                                              |  
| min          | num, num              | num         | minimum of arguments                                                              |  
| min          | string, string        | string      | minimum of arguments                                                              |  
| peekl        | int                   | int         | reads 31 bit long from address *int*                                              |  
| peekw        | int                   | int         | reads 16 bit word from address *int*                                              |  
| pi           |                       | real        | variable containing pi = 3.1415926535897932384626433                              |  
| pokel        | int *addr*, int *long*| nil         | writes 31 bit *long* to *addr*                                                    |  
| pokew        | int *addr*, int *word*| nil         | writes 16 bit *word* to *addr*                                                    |  
| rad          | num                   | real        | convert from degrees to radians                                                   |  
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


Trailing `?` indicates optional parameter. `num` means any number type, `int` or `real`.

