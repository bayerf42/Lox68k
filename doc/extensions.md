# Lox68k extensions to the original Lox language definition

### Glossary

  * → means *successfully evaluates to*
  * ⟿ means *raises exception*


## List datatype
A new data type List has been added. It is a dynamic array of any Lox values, which is indexed
by a numeric index ranging from 0 to the length of the array minus 1. Since lists are mutable
values, you can assign to an indexed location in a list.

Lists can be created in several ways, by the `list` native which creates a list of *n* elements
optionally initializing it with a value. 

Alternatively, lists can be created by writing `[` and then a comma-separated list of expressions,
terminated by `]`. 

```javascript
  var lst = [2, 5, nil, "foo", sinh];
  lst → [2, 5, nil, "foo", <native sinh>];

  list(5) → [nil, nil, nil, nil, nil]
  list(3, 17+4) → [21, 21, 21]
```

Lists can be both treated in a functional way creating new lists with `+`, `list`, `reverse`
and by modifying existings lists: `append`, `delete`, `insert`, assigning to an index.

```javascript
  lst + reverse([1, 2, 3]) → [2, 5, nil, "foo", <native sinh>, 3, 2, 1];

  insert(lst, 2, "bar") → nil
  lst → [2, 5, "bar", nil, "foo", <native sinh>]

  delete(lst, -2) → nil
  lst → [2, 5, "bar", nil, <native sinh>]

  append(lst, "last") → nil
  lst → [2, 5, "bar", nil, <native sinh>, "last"]
```



## Indexing lists and strings
Both Lox68k's sequence types (lists and strings) can be indexed by a numerical index, with either
goes from 0 to the length of the sequence minus one (when indexing from the left), or from
minus 1 to minus the length (when indexing from the right). So index 0 is the first element, and
index -1 is the last element of the sequence. It is an error when an index is beyond those limits
and yields the error message `index out of range`.

```javascript
  var text = "Hello, world";

  length(text) → 12
  text[4] → "o" // Since Lox68k hasn't a char type, single chars are strings with length 1
  text[-3] → "r" // just like in Python
  text[15] ⟿ "String index out of range."
  text[true] ⟿ "String index is not an integer."

  text[2] = "x" ⟿ "Can't store into type string."
```

```javascript
  var lst = [2, 5, nil, "foo", sinh];

  length(lst) → 5
  lst[4] → <native sinh>
  lst[-3] → nil
  lst[15] ⟿ "List index out of range."
  lst[true] ⟿ "List index is not an integer."

  lst[2] = "ok" → "ok" // list items may be assigned

  lst → [2, 5, "ok", "foo", <native sinh>];
```


## Slicing lists and strings
Additionally to indexing, you can extract subsequences of Lox68'x sequence types (lists and strings)
by giving lower and upper limits in slice syntax `[` lower `:` upper`]`. The lower limit is
inclusive, the upper is exclusive. If a limit is omitted, it defaults to the beginning or end of the
sequence.

Just like with indexing, positive and negative limits may be specified.
In contrast to simple indices, slicing indices may be beyond the actual limits of a sequence and
default to the maxima.

```javascript
  var text = "Hello, world";

  text[2:5] → "llo"  // start at (2+1) upto 5th
  text[:5] → "Hello" // first 5 first characters
  text[-3:] → "rld" // last 3 characters
  text[:] → "Hello, world" // a copy, but actually identical due to string interning
  text == text[:] → true // verification of previous claim
```

```javascript
  var lst = [2, 5, nil, "foo", sinh, [], Set];

  lst[2:5] → [nil, "foo", <native sinh>]
  lst[:5] → [2, 5, nil, "foo", <native sinh>]
  lst[-3:] → [<native sinh>, [], <class Set>]
  lst == lst[:] → false // copy is a new object, never identical to original
```
Assignment to a slice is not allowed.

## Classes and instances

Classes are first-class values in Lox, so some restrictions have been removed: A class can
be derived from any expression evaluating to a class, so some meta-class operations are available:

```javascript
fun counted(clazz) {
    var num = 0;

    class Deco < clazz {        // local class inheriting from function parameter
        init(..args) {          // see below for variadic parameters
            super.init(..args); // init superclass with original parameters
            num = num + 1;      // increment the instance counter which is an upvalue shared
                                // among all instances
        }

        num_instances() -> num  // method shorthand (see below) returning number of instances
    }
    return Deco;                // returned class includes everything from class and counts
                                // its instances
}

var CountedSample = counted(Sample); // Assume Sample is a class, we create a counted variant of it.

var s1 = CountedSample(4);      // create an instance, automatically incrementing instance counter
var s2 = CountedSample(5);      // dito
var s3 = CountedSample(8);      // dito

s2.num_instances() → 3                  // 3 instances have been created
s3.num ⟿ "Undefined property 'num'."  // num is an upvalue, not a slot of the instance!

class_of(s1) → <class Deco>           // some native functions for metaclass fun!
parent(class_of(s1)) → <class Sample>
```

Instances can be accessed by an index written in square brackets, accessing the
hashtable of the instance. But in contrast to the `.` property access, it can access any key type,
not only strings. Also the access with the indexing operator `[]` doesn't default to the class's
methods when a slot doesn't exist.

```javascript
  var map = Map("foo",42, "bar",38, true,666);

  map.bar → 38
  map["bar"] → 38
  map[bar] ⟿ "Undefined variable 'bar'."

  map.zyz ⟿ "Undefined property 'zyz'."
  map["zyz"] → nil
  map["zyz"] = 111 → 111 // assignment always works
  map.zyz → 111

  map.string → <bound Map.string> // a method of the class (defined in stdlib.lox) 
  map["string"] → nil // but not in the instance

  map.true ⟿ "Expect property name after '.'." // true is a keyword, not an identifier
  map[true] → 666 // key is bool true
  map["true"] → nil // key is not the string "true"

  map → Map("foo",42, "bar",38, "zyz",111, true,666) // 'zyz' property has been added in 2nd block
```

## Iterators
An iterator is a new Lox builtin type allowing iterating over all slots of an instance.

```javascript
  var map = Map("foo",42, "bar",38, true,666);

  // this is the standard idiom to iterate over all items in a dictionary.
  for (var iter = slots(map); valid(iter); next(iter)) {
      print iter@;  // the @ operator accesses the key, read-only
      print iter^;  // the ^ operator accesses the value, may assign to it
  }
```



## Functions

### Variadic functions
The last parameter name of a function may be prefixed by `..` indicating that this is a
rest parameter, collecting all function arguments into a list.

In a function call, `..` can be prefixed to an expression (which must evaluate to a list) and
this list is spliced into the argument list. This is allowed at any position in the argument
list and may even occur several times. The called function does not need to be variadic, it is
only required that it accepts the number of arguments it is called with.

```javascript
  fun foo(a, b, c, ..more) {
    return [b, length(more)];
  }

  foo(1, 2, 3, 4, 5) → [2, 2]
  foo(1, 2, 3)       → [2, 0]
  foo(1, 2)          ⟿ "'foo' expected at least 3 arguments but got 2."
  foo(1, ..list(4), 4, 5, ..list(3), 6, 7) → [nil, 9] // 5 fixed args, 7 args from lists
                                                      // 3 comsumed by fixed parameters, 9 left

  [1, 2, ..list(4, 3), 6, 7, ..list(2, 9), list(2, 11), 14] → [1, 2, 3, 3, 3, 3, 6, 7, 9, 9, [11, 11], 14]
  // .. splicing works the same way in list constructors

```
  
Named function parameters are not allowed.

### Anonymous functions (lambdas)
### Shorthand


## Native functions protocol





## `case` statement
The `case` statement provides a multi-way branch. It evaluates an expression and compares its value
to a list of other values and selects the first matching branch.

## `break` statement
The `break` statement is very simple. It leaves the innermost `for` or `while` loop immediately
cleaning up all lexical variables and jumps to the statement following the loop.


## Exception handling
There is no sophisticated exception class hierarchy, all Lox runtime errors are simple strings.

## Dynamic variables


## Debugging utilities

### Bytecode disassembler
### Execution statistics
### Tracing calls
### Tracing VM steps
### Tracing garbage collection

 
## Minor extensions

### Equality vs. Identity

### Numeric literals

#### Hexadecimal
By prefixing `$` a  hexadecimal integer can be specified. Both lower and upper digits are
allowed.
```javascript
  $AB → 171
  $fffffffe → -2
  $3fffffff → 1073741823 // maxint (31 bits)
  $40000000 → -1073741824 // minint (31 bits)
  hex(255) → "ff"
```


#### Binary
A binary literal is introduced by `%` and includes all '0' and '1' chars thereafter.
```javascript
  %11111 → 31
  %101 → 5
  bin(255) → "11111111"
  % ⟿ "No digits after radix."
  poke($f0000, %10101010) → nil // and sets debug LEDs
```


#### Exponents
Real numbers can have a decimal exponent in a standard way
```javascript
  1e3 → 1000.0 // reals are always printed with a decimal point to distinguish them from ints
  4.56e-5 → 4.56e-005
  23e ⟿ "Empty exponent in number."
```

### Print lists
