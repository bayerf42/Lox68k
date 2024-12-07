# Lox68k extensions and changes to the original Lox language definition

### Goals
These were my goals, in decreasing order of importance.

  * Have fun
  * Build a modern dynamic scripting language implementation for a 45 year old architecture
  * Make it run on a [Motorola 68008 SBC](https://kswichit.net/68008/68008.htm)
  * Keep it small, simple, and elegant; stay below 64k code size
  * Improve my compiler/interpreter knowledge 
  * Focus on functional style more than OOP
  * Make it useful
  * Make it fast on 68000
  * Improve my Github portfolio
  * Make it run on other platforms, currently for testing and comparison only.   

### Glossary

  * → means *successfully evaluates to*
  * ⟿ means *raises exception*
  * ⇒ means *prints to terminal*

Note that keyword highlighting in the code samples is not correct, since there is no Lox
predefined syntax highlighter and the JS one has been used, which is close but not 100% correct.


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
and by modifying existings lists: `append`, `delete`, `insert`, and assigning to an index.

```javascript
  lst + reverse([1, 2, 3]) → [2, 5, nil, "foo", <native sinh>, 3, 2, 1];

  insert(lst, 2, "bar") → nil
  lst → [2, 5, "bar", nil, "foo", <native sinh>]

  delete(lst, -2) → nil // see below for negative index
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
by giving lower and upper limits in slice syntax `[` *lower* `:` *upper*`]`. The lower limit is
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

You shouldn't use a real number as an index (see below at *Equality vs. Identity*)

To remove a slot, use the `remove` native function, assigning `nil` to a slot (like in Lua)
doesn't remove it.

```javascript
  // The class Map in the standard library may be called with an even number of arguments
  // building a map from key,value pairs.
  var map = Map("foo",42, "bar",38, true,666);

  map.bar → 38
  map["bar"] → 38
  map[bar] ⟿ "Undefined variable 'bar'."

  map.zyz ⟿ "Undefined property 'zyz'."
  map["zyz"] → nil // but index returns nil, not an error!
  map["zyz"] = 111 → 111 // assignment always works
  map.zyz → 111

  map.string → <bound Map.string> // a method of the class (defined in stdlib.lox) 
  map.clone → <bound Object.clone> // another method, but defined in the superclass 'Object' 
  map["string"] → nil // method is not defined in instance

  map.true ⟿ "Expect property name after '.'." // true is a keyword, not an identifier
  map[true] → 666 // key is bool true
  map["true"] → nil // key is not the string "true", no value found

  map → Map("foo",42, "bar",38, "zyz",111, true,666) // 'zyz' property has been added in 2nd block
  // Note the spacing indicating key,value pairs. This output is valid input to the REPL
  // recreating an equal Map.
```

## Iterators
An iterator is a new Lox builtin type allowing iterating over all slots of an instance.

```javascript
  var m = Map("foo",42, "bar",38, true,666);

  // this is the standard idiom to iterate over all slots in an instance.
  for (var iter = slots(m); valid(iter); next(iter)) {
      print iter@,,;     // the @ operator accesses the key, read-only
      print iter^;       // the ^ operator accesses the value, may assign to it
      remove(m, iter@);  // removing the current item is allowed
  }
  ⇒ bar   38    // iteration order depends on key hash, consider it random
  ⇒ true   666
  ⇒ foo   42
  m → Map()   // empty, all items have been removed in loop
```
You can clone an iterator which moves independently from its origin with `it_clone()`. This
may be useful for some quadratic algorithms.
 
## Functions

### Variadic functions, rest parameter, argument list splicing
The last parameter name of a function may be prefixed by `..` indicating that this is a
rest parameter, collecting all function arguments into a list.

In a function call, `..` can be prefixed to an expression (which must evaluate to a list) and
this list is spliced into the argument list. This is allowed at any position in the argument
list and may even occur several times. The called function does not need to be variadic, it is
only required that it accepts the number of arguments it is called with.

```javascript
  fun foo(a, b, c, ..more) { // 4 parameters, but actually accepts at least 3, ..more may be empty
    return [b, length(more)];
  }

  foo(1, 2, 3, 4, 5) → [2, 2]
  foo(1, 2, 3)       → [2, 0]
  foo(1, 2)          ⟿ "'foo' expected at least 3 arguments but got 2."
  foo(1, ..list(4), 4, 5, ..list(3), 6, 7) → [nil, 9] // 5 fixed args, 7 args from lists
                                                      // 3 consumed by fixed parameters, 9 left

  [1, 2, ..list(4, 3), 6, 7, ..list(2, 9), list(2, 11), 14] → [1, 2, 3, 3, 3, 3, 6, 7, 9, 9, [11, 11], 14]
  // .. splicing works the same way in list constructors

```
  
Named function parameters are not allowed.

### Anonymous functions (lambdas)
An anonymous function is written like a normal function declaration, but omitting the name.
It can be used anywhere an expression is allowed. Its value is a closure capturing variables
from its lexical environment (just like a named function) and gets a sequential number for its
name. The body can either be a block or (more frequently) the `->` shorthand described below.

```javascript
  map(fun (x) -> x*x, [2,3,4]) → [4, 9, 16]

  (fun (a,b) {}) → <closure #7> // must put lambda expression in parentheses; otherwise
                                // the compiler would expect a function declaration after
                                // the keyword 'fun' and complain about missing function name
```

### Shorthand syntax ->
If a function body just returns an expression, the whole body can be abbreviated with an arrow.
```javascript
  fun cube(x) {
    return x*x*x;
  }
  // can also be written
  fun cube(x) -> x*x*x
```
This syntax is orthogonal to lambdas, it can be used for named functions and methods, too.

### Native functions protocol
There is a standard protocol for calling native (implemented in C) functions from Lox which
cares for argument count and type checking, signalling success or failure to the Lox VM.
See `native.c` for details.

## `if` expression
An `if` expression works exactly like the ternary operator `?:` from C or Javascript (which
is clumsy IMHO, so I changed the syntax here), but is written in a function-like syntax. To
emphasize the special evaluation order, the arguments are separated by colons instead of commas.

```javascript
  fun max(a, b) -> if(a>b : a : b)
```

## `break` statement
The `break` statement is very simple. It leaves the innermost `for` or `while` loop immediately
cleaning up all lexical variables and jumps to the statement following the loop.


## `case` statement
The `case` statement provides a multi-way branch. It evaluates an expression and selects
the first matching branch.

A branch is introduced by `when` and followed by a comma-seperated list of comparison values
(needn't be constant). When a branch is selected, all following statements are executed and
the `case` statement is left. No `break` statement is needed to leave it, in fact, `break` would
leave the loop containing the `case` statement. 

When no branch matches the `case` expression, nothing is executed, unless there is a
final `else` branch.

```javascript
  fun daysInMonth(month, year) {
    case (lower(month)) {
      when "feb":                      return if(year\4 == 0 : 29 : 28); // simplified
      when "jan", "mar", "may", "jul",
           "aug", "oct", "dec":        return 31;
      when "apr", "jun", "sep", "nov": return 30;
      else                             error("Invalid month name " + month);
    }
  }
```


## Exception handling
When (original) Lox encounters a run-time error, the error message including a backtrace of
function names and line numbers is printed and the evaluation is aborted.

Lox68k allows handling these errors with the `handle` expression. The first sub-expression is evaluated
in a context where any exception raised calls the other expression (the error handler)
with the error message as its single argument.
Like in the `if` expression, a colon must be written between the expressions to
emphasize the special evaluation order.

```javascript
  handle(5/0 : upper) → "DIV BY ZERO."
```
One can also manually raise an exception with the `error` native whose argument can have any
type. This argument is passed unchanged to an active handler, but system errors have always
type `string`. Grep Lox68k sources for `runtimeError` to see all possible system errors.

The value of a `handle` expression is the value of its first argument if no exception
has been raised, or the value returned from the error handler otherwise.

If you want to discriminate between errors, you have to analyse the error string yourself, or
raise exceptions of non-string type. You can re-raise the exception in the handler, which causes
searching the next handler up the call stack.

To keep things simple, there is no sophisticated class hierarchy for exceptions
like in other languages, the first handler encountered is called always.

For the same reason, exception handling is implemented for expressions, not for statements
(`try/catch`), since statements complicate the control flow in exception handling. 


```javascript
// Implement even/odd by mutual recursion, give back result via exception handling
fun makeEven() {
  var even, odd; // forward declarations
  odd  = fun (n) -> if(n==0 : error(false) : even(n-1));
  even = fun (n) -> if(n==0 : error(true)  : odd(n-1));
  return fun (n) -> handle(even(n) : fun (exc) -> if(type(exc)=="bool" : exc : error(exc)));
}

var even = makeEven();

// You may want to trace call frames by setting dbg_call(true) here

even(8)      → true  // from deep call via exception, where bools are handled and returned. 
even(9)      → false // same.
even(1000)   ⟿ "Lox call stack overflow."             // System error, re-raised in handler
even("bla")  ⟿ "Can't subtract types string and int." // same.
```


## Dynamic variables
Inspired by Common Lisp's special variables:
* Lexical variables have **lexical scope**, they are visible in certain program regions only,
  determined  at compile time, but exist for **indefinite extent** during run time 
  (even when the defining function has returned, so-called *upvalue*)
* Dynamic variables have **indefinite scope**, they are visible anywhere in the program,
  but exist only for a limited time period (**dynamic extent**) during run time

A Lox variable is resolved lexically, if it is declared locally or in an enclosing function.
If it isn't found at compile time, it is assumed to be a global variable, which is resolved
dynamically at runtime. To define a global variable in Lox, a `var` statement must be
executed at top level in a script. Global variables exists forever until the REPL is terminated.

Lox68k extends this scheme, a dynamic variable can be defined in a `dynvar` expression and
then exists during evaluation of the following expression, automatically being removed afterwards.
When there already is a global variable with the same name, the new value temporarily shadows
the original during evaluation. Of course those shadowings may be nested.

```javascript
  // Physics: Compute altitude in free fall.
  fun s(t) -> 0.5 * g * t * t        // t is lexical (parameter), g is dynamic
  s(3) ⟿ "Undefined variable 'g'." // Acceleration 'g' not yet defined
  var g = 9.81;                      // Make Earth's acceleration the default.
  s(3)                  → 44.145     // Altitude after 3s fall.
  dynvar(g=1.62 : s(3)) →  7.29      // Temporarily falling on the moon.
  s(3)                  → 44.145     // We are back on earth.
```
Another application of dynamic variables is instrumentation of code:

```javascript
  // define a wrapper to log function calls
  fun logging(f) -> 
    fun(..args) {
      print "Calling ", f, " with args ", args,;
      var res = f(..args);
      print " gives ", res;
      return res;
    }

  fun hypot(a,b) -> sqrt(pow(a,2)+pow(b,2)) // 'pow' is a native function, defined globally

  // Rebind 'pow' to its logging version during testing 
  fun test(a,b) -> dynvar(pow=logging(pow) : hypot(a,b)) 

  test(5,12) ⇒ Calling <native pow> with args [5, 2] gives 25.0
             ⇒ Calling <native pow> with args [12, 2] gives 144.0
             → 13.0
```
Lexical variables are the default and usually better than dynamic variables, but dynamic variables
are occasionally useful and elegant. 


## Debugging utilities
All debugging utilities are only available when Lox68k is compiled with the symbol `LOX_DBG`
defined. Then they include the ones given in the book and a few more, but all debugging options
are separately switchable by calling the corresponding function with a truish or falsey argument.

### Bytecode disassembler
Control disassembling the generated byte code after compilation with the switch
`dbg_code(arg)`.

### Execution statistics (*new*)
Control printing execution statistic after an evaluation with the switch
`dbg_stat(arg)`. Printed are
  * real time used 
  * number of virtual machine instructions 
  * number of bytes allocated
  * number of garbage collections

### Tracing calls (*new*)
Control tracing of calls to closures with the switch `dbg_call(arg)`. Each entry to a closure
is logged with all arguments in a single line, which is prefixed by `-->` and indented according
to the call nesting.

On closure exit, `<--` and the returned value are printed, also indented.

To control tracing of calls to native functions, use the switch `dbg_nat(arg)`.
Arguments and results of every call to a native functions are printed in a single line,
starting with `---`, only indented when also tracing closure calls.

Summary of tracing indicators:
  * `-->` calling a closure
  * `<--` returning from a closure call
  * `==>` established an exception handler
  * `<==` an exception is being raised
  * `~~>` binding a dynamic variable
  * `---` calling a native function, `->` result, *unless:*
  * `/!\` native function raised exception

### Tracing VM steps
Control tracing every VM step with the switch `dbg_step(arg)`. This is like in the book
and creates huge amount of output.

### Tracing garbage collection
`dbg_gc(bits)` in contrast expects a bit mask (see `memory.h` for details) to select logging
several garbage collector messages. This may also create  lots of output.
 

## Equality vs. Identity
The original Lox `number` type has been split into `int` and `real`, since the 68k chip only
supports hardware integers and floating point numbers have to be emulated in software which is
quite slow.

Logically, the types `nil`, `bool`, `int`, `real`, `string`, and `native` are value types,
they have no identity and can't be modified. All other types (`list`, `closure`, `class`, `instance`,
`bound`, `iterator`) are reference types with an identity and are modifiable.

The `==` operator compares value types via their values and reference types via their identity,
which is actually a simple 32 bit comparison (`valuesEqual()` macro).
This comparison applies also to `case` branch selection and instance keys.

Implementation-wise, natives, strings and reals are objects in the heap, but (mostly)
behave like value types, since:
  * Natives are constants (pointers into ROM), there's no way to create a new native from Lox.
  * Strings are always interned, so a duplicate string is in fact the *identical* object.
  * Reals are not interned, so `1.2 == 1.2` is false (two distinct objects),
    but **reals shouldn't be compared for equality anyway**. And don't use reals as instance
    keys or `case` branch selectors. 

However, if you really want to compare reals for numeric equality, you can use the `equal`
native, which also does type conversion, so `equal(3, 3.0)` is true.

## Minor extensions

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

### Printing
`print` remains a statement, not a native function, so some syntactic variants are possible.
`print` allows any number of expressions to be printed, if they are separated by a single comma,
their values are printed without space, if separated by two commas 3 spaces are inserted between.
Strings are printed without quotes. All values are printed in a single line. If the list ends
with a comma, the newline is suppressed and the next `print` statement continues at the same line.
`print` may be abbreviated by `?`

```javascript
  print "Result is",7; ⇒ Result is7
  print "Result is",,7,,11; ⇒ Result is   7   11
  ? 42; ⇒ 42

```

### Declaring multiple variables
Multiple variables can be declared and initialized in a single `var` declaration.
```javascript
  var a=5, b=2+a, c, d=14-b; // Later variables may refer to earlier ones.
```
