# Lox68k standard library

The Lox standard library contains several useful functions and classes which are written
in Lox itself. There are 2 slightly different versions (due to memory alignment and byte order)
for 68k and Intel/ARM architectures, files `lox/stdlib_68k.lox` and `lox/stdlib.lox`.

You can start Lox68k with or without the standard library:

* On the Kit start at `$44000` for Lox [debug] *with* the standard library, or at `$44008` for Lox
  *without* the standard library. The standard library is bundled automatically when
  building the ROM image.

* If you want the faster non-debug version, start it at `$52000`, or at `$52008` likewise.

* On the 68k Simulator the standard library is loaded from the ROM image `../roms/mon_ffp_lox.bin`
  on start-up. You can use either start address like above or just press the *RUN* icon.

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
| complement   | fun                   | fun         | boolean complement of *fun*                                                       |  
| const        | any                   | fun         | creates a function which always returns *any*                                     |  
| deg          | num                   | real        | convert from radians to degrees                                                   |  
| filter       | fun, seq              | list        | filter all items of *seq* which satisfy predicate *fun*                           |  
| id           | any                   | any         | returns its argument                                                              |  
| instance     | any, class            | bool        | true, if *any* is an instance of *class*                                          |  
| list_globals | -                     | nil         | prints all global variables                                                       |  
| map          | fun, seq              | list        | apply *fun* to every item in *seq*, return list of results.                       |  
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
| reduce       | fun, seq              | any         | combine all items of *seq* with operator *fun* from the left                      |  
| string       | any                   | string      | convert *any* to a string, using *string* method for objects.                     |  


## Classes

| Name        | Methods   | Parameters    | Returns     | Description                                                                       |
|-------------|-----------|---------------|-------------|--------------|--------------------------------------------------------------------|
| Object      |           | -             |             | base class for every Lox class, defines string() and clone() methods.             |
|             | clone     | -             | instance    | method used to shallow-copy an instance                                           |  
|             | string    | -             | string      | method used to convert an object to a string                                      |  
| Map         |           | any*          |             | a map containing key/value pairs, making the internal hash table explicit         |
| Set         |           | any*          |             | a set implemented as a hash table                                                 |
|             | intersect | Set           | Set         | intersect *Set* with receiver, modifying it                                       |  
|             | subtract  | Set           | Set         | subtract *Set* from receiver, modifying it                                        |  
|             | union     | Set           | Set         | unite *Set* with receiver, modifying it                                           |  


Trailing `?` indicates optional parameter. `num` means any number type, `int` or `real`,
`seq` is any sequence, `list` or `string`.

