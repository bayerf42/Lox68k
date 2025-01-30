# Lox68k extensions and changes to the original Lox language definition

### Goals
These were my goals, in decreasing order of importance.

  * Have fun
  * Build a modern dynamic scripting language implementation for a 45 year old architecture
  * Make it run on a [Motorola 68008 SBC](https://kswichit.net/68008/68008.htm)
  * Keep it small, simple, and elegant; stay below 64k code size
  * ... but provide good error messages nevertheless
  * Improve my compiler/interpreter knowledge 
  * Focus on functional style more than OOP
  * Provide a small but useful set of [datatypes](operations.md)
    and [native functions](natives.md)
  * Make it fast on 68000
  * Improve my [Github portfolio](https://github.com/bayerf42)
  * Make it run on other platforms, currently for testing and comparison only.   

### Glossary

  * → means *successfully evaluates to* and prints implicitly
  * ✪ means *compiler error*
  * ⚠ means *runtime exception*
  * ✎ means *prints to terminal* by an explicit `print` statement

Lox68k allows evaluating both declarations/statements and expressions at top-level,
see [below for details.](#toplevel)

```javascript
  6*7;        // interpreted as an expression statement, result is ignored, nothing printed
  6*7   → 42  // without trailing ';' it is an expression, printing its value implicitly
  print 6*7;  // a statement printing the expression value explicitly
    ✎ 42
  6*+   ✪ "Error at '+': Expect expression."  // Syntax error detected by compiler
  6*nil ⚠ "Can't multiply types int and nil." // Runtime error detected by VM 
```

*Lox* refers to the original language definition, *Lox68k* to my extended version.

Note that keyword highlighting in the code samples is not correct, since there is no Lox
predefined syntax highlighter and the JS one has been used, which is close but not 100% correct.

In the samples below, it is assumed that the [standard library](stdlib.md) has been loaded.

## <a id="list"></a>List datatype
A new data type List has been added. It is a dynamic array of any Lox values, which is indexed
by an integer index. Since lists are mutable values, you can assign to an indexed location
in a list.

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


## <a id="index"></a>Indexing lists and strings
Both Lox68k's sequence types (lists and strings) can be indexed by an integer index, with either
goes from 0 to the length of the sequence minus one (when indexing from the left), or from
minus 1 to minus the length (when indexing from the right). So index 0 is the first element, and
index -1 is the last element of the sequence. It is an error when an index is beyond those limits
and yields the error message `index out of range`.

```javascript
  var text = "Hello, world";

  length(text)  → 12
  text[4]       → "o" // There is no char type, represent chars as strings of length 1,
  text[-3]      → "r" // just like in Python
  text[15]      ⚠ "String index out of range."
  text[true]    ⚠ "String index is not an integer."

  text[2] = "x" ⚠ "Can't store into type string."
```

```javascript
  var lst = [2, 5, nil, "foo", sinh];

  length(lst)   → 5
  lst[4]        → <native sinh>
  lst[-3]       → nil
  lst[15]       ⚠ "List index out of range."
  lst[true]     ⚠ "List index is not an integer."

  lst[2] = "ok" → "ok" // list items may be assigned
  lst           → [2, 5, "ok", "foo", <native sinh>];
```


## <a id="slice"></a>Slicing lists and strings
Additionally to indexing, you can extract subsequences of Lox68'x sequence types (lists and strings)
by giving lower and upper limits in slice syntax `[` *lower* `:` *upper*`]`. The lower limit is
inclusive, the upper is exclusive. If a limit is omitted, it defaults to the beginning or end of the
sequence.

Just like in indexing, positive and negative limits may be specified.
In contrast to simple indices, slicing indices may be beyond the actual limits of a sequence and
default to the maxima.

```javascript
  var text = "Hello, world";

  text[2:5]       → "llo"          // start at 2 upto (excluding) 5th
  text[:5]        → "Hello"        // first 5 characters
  text[-3:]       → "rld"          // last 3 characters
  text[:]         → "Hello, world" // a copy, but actually identical due to string interning
  text == text[:] → true           // verification of previous claim
```

```javascript
  var lst = [2, 5, nil, "foo", sinh, [], Set];

  lst[2:5]        → [nil, "foo", <native sinh>]
  lst[:5]         → [2, 5, nil, "foo", <native sinh>]
  lst[-3:]        → [<native sinh>, [], <class Set>]
  lst == lst[:]   → false          // copy is a new list, never identical to original
```
Assignment to a slice is not allowed.

## <a id="string"></a>Some native functions for strings

### Splitting
To split a string into a list of substrings at a set of separator characters, use the `split`
native.

```javascript
  var str = "Lorem ipsum dolor sit amet,  consetetur elitr. ";

  split(str, " ")   → ["Lorem", "ipsum", "dolor", "sit", "amet,", "consetetur", "elitr."]
  split(str, " ,.") → ["Lorem", "ipsum", "dolor", "sit", "amet", "consetetur", "elitr"]
  split(str, "t")   → ["Lorem ipsum dolor si", " ame", ",  conse", "e", "ur eli", "r. "]
```
In contrast to the `split` function in other languages, sequences of more than one
separator characters are removed at once, so there'll never be an empty string
in the result list.

### Joining
To join a list of strings without creating lots of temporary strings, use the `join` native.
It has optional parameters to insert a separator string between list elements and to add
prefix and postfix strings. 

```javascript
  var lst = ["2025", "Jan", "15"];

  join(lst)                    → "2025Jan15"
  join(lst, "-")               → "2025-Jan-15"
  join(lst, "-", "<<", ">>")   → "<<2025-Jan-15>>"
  join(["foo", 42])            ⚠ "'join' string expected in list at 1."
```

### <a id="regex"></a>Regular expressions
Most regular expression matching libraries are very big, larger than the entire Lox68k
implementation. So the very small (only 30 lines of C!) and elegant
[matching routine by Rob Pike](https://www.cs.princeton.edu/courses/archive/spr09/cos333/beautiful.html)
has been employed, which supports a restricted` but nevertheless useful subset of meta characters:

* `c` matches any literal character c
* `.` matches any single character
* `^` matches the beginning of the input string
* `$` matches the end of the input string
* `*` matches zero or more occurrences of the previous character
* `?` matches zero or one occurrence of the previous character (new)

Omitting meta `+` saves some code and is no problem, just write `cc*` instead of `c+`.

On successful match, a list with begin and end positions is returned. The begin position
is the (0-based) index into the searched string, the end position is the position of the
first character *not* matched. Both position can be the length of the string when an empty
match is found at the end. These positions can be used in a slice expression to extract
the matched part of the string.

When the pattern doesn't match, `nil` is returned.

Just like with the `index` native, you can specify an optional start position for the search.
Nevertheless, the returned positions refer to the original string.
 

```javascript
  match("xy",   "abxycde") → [2, 4]
  match("xz",   "abxycde") → nil
  match(".y",   "abxycde") → [2, 4]
  match(".*y",  "abxycde") → [0, 4]
  match("b$",   "abcdb")   → [4, 5]
  match("ab*c", "xacz")    → [1, 3]
  match("ab*c", "xabbbcz") → [1, 6]
  match("ab?c", "xabbbcz") → nil
  match("",     "foo")     → [0, 0] // empty always matches at start
  match("$",    "foo")     → [3, 3] // empty match at end

  fun extract(re, str) {
    var range = match(re, str);
    if (range)
      return str[range[0] : range[1]];
    else
      error("No match.");
  }

  extract("ab*c", "xabbbcz") → "abbbc"
  extract("ab*c", "xabbbz")  ⚠ "No match."

  match("a*b", "xaaabyabz")    → [1, 5]
  match("a*b", "xaaabyabz", 5) → [6, 8]

```

## <a id="class"></a>Classes and instances

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
    return Deco;                // returned class includes everything from clazz and counts
                                // its instances
}

var CountedSample = counted(Sample); // Assume Sample is a class, create a counted variant.

var s1 = CountedSample(4);      // create an instance, automatically incrementing counter
var s2 = CountedSample(5);      // dito
var s3 = CountedSample(8);      // dito

s2.num_instances()   → 3                           // 3 instances have been created
s3.num               ⚠ "Undefined property 'num'." // num is an upvalue, not a slot!

class_of(s1)         → <class Deco>                // some native functions for metaclass fun!
parent(class_of(s1)) → <class Sample>
```

Instances can be accessed by an index written in square brackets, accessing the
hashtable of the instance. But in contrast to the `.` property access, it can access any key type,
not only strings. Also the access with the indexing operator `[]` doesn't default to the class's
methods when a slot doesn't exist.

You shouldn't use a real number as an index (see below at *Equality vs. Identity*)

To remove a slot, use the `remove` native function; assigning `nil` to a slot (like in Lua)
doesn't remove it.

```javascript
  // The class Map in the standard library may be called with an even number of arguments
  // building a map from key,value pairs.
  var map = Map("foo",42, "bar",38, true,666);

  map.bar          → 38 // like in 
  map["bar"]       → 38 // Lua and JavaScript
  map[bar]         ⚠ "Undefined variable 'bar'."

  map.zyz          ⚠ "Undefined property 'zyz'."
  map["zyz"]       → nil // but index returns nil, not an error!
  map["zyz"] = 111 → 111 // assignment always works
  map.zyz          → 111

  map.string       → <bound Map.string>   // a method of the class 
  map["string"]    → nil                  // method is not defined in instance
  map.clone        → <bound Object.clone> // another method, but defined in superclass 

  map.true         ✪ "Expect property name after '.'." // true is a keyword, not an identifier
  map[true]        → 666 // key is bool true
  map["true"]      → nil // key is not the string "true", no value found

  map              → Map("foo",42, "bar",38, "zyz",111, true,666)
  // 'zyz' property has been added in 2nd block
  // Note the spacing indicating key,value pairs. This output is valid input to the REPL
  // recreating an equal Map.
```

## <a id="iterator"></a>Iterators
An iterator is a new Lox builtin type allowing traversing all slots of an instance.

```javascript
  var m = Map("foo",42, "bar",38, true,666);

  // this is the standard idiom to iterate over all slots in an instance.
  for (var iter = slots(m); valid(iter); next(iter)) {
      print iter@,,;     // the @ operator accesses the key, read-only
      print iter^;       // the ^ operator accesses the value, may assign to it
      remove(m, iter@);  // removing the current item is allowed
  }
    ✎ bar   38           // iteration order depends on key hash, consider it random
    ✎ true   666
    ✎ foo   42
  m → Map()              // empty, all items have been removed in loop
```
You can clone an iterator which moves independently from the original with `it_clone()`. This
may be useful for some quadratic algorithms.
 
## Functions

### <a id="variadic"></a>Variadic functions, rest parameter, argument list splicing
The last parameter name of a function may be prefixed by `..` indicating that this is a
rest parameter, collecting all remaining function arguments into a list.

In a function call, `..` can be prefixed to an expression (which must evaluate to a list) and
this list is spliced into the argument list. This is allowed at any position in the argument
list and may even occur several times. The called function does not need to be variadic, it is
only required that it accepts the number of arguments it is called with.

```javascript
  fun foo(a, b, c, ..more) {
    // 4 parameters, but actually accepts at least 3, '..more' may be empty
    return [b, length(more)];
  }

  foo(1, 2, 3, 4, 5) → [2, 2]
  foo(1, 2, 3)       → [2, 0]
  foo(1, 2)          ⚠ "'foo' expected at least 3 arguments but got 2."
  foo(1, ..list(4, true), 4, 5, ..list(3), 6, 7)
    → [true, 9] // 5 fixed args, 7 spliced args,
                // 3 consumed by fixed parameters, 9 left for rest parameter

  // '..' splicing works the same way in list constructors
  [1, 2, ..list(4, 3), 6, 7, ..list(2, 9), list(2, 11), 14]
    → [1, 2, 3, 3, 3, 3, 6, 7, 9, 9, [11, 11], 14]

```
  
Named function parameters are not supported.

### <a id="lambda"></a>Anonymous functions (lambdas)
An anonymous function is written like a normal function declaration, but omitting the name.
It can be used anywhere an expression is allowed. Its value is a closure capturing variables
from its lexical environment (just like a named function) and gets a sequential number as its
name (for printing).
The body can either be a block or (more frequently) the `->` shorthand described below.

```javascript
  map(fun (x) -> x*x, [2,3,4]) → [4, 9, 16]

  (fun (a,b) {}) → <closure #7> // must put lambda expression in parentheses; otherwise
                                // the compiler would expect a function declaration after
                                // the keyword 'fun' and complain about missing function name
```

### <a id="arrow"></a>Shorthand syntax `->`
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

## <a id="if_expr"></a>`if` expression
An `if` expression works exactly like the ternary operator `?:` from C or Javascript (which
is clumsy IMHO, so I changed the syntax here), but is written in a function-like syntax. To
emphasize the special evaluation order, the arguments are separated by colons instead of commas.

```javascript
  fun max(a, b) -> if(a>b : a : b)
```
As you see here, it combines nicely with the `->` syntax.

## <a id="break"></a>`break` statement
The `break` statement is very simple. It leaves the innermost `for` or `while` loop immediately
closing open scopes and jumps to the statement following the loop.


## <a id="case"></a>`case` statement
The `case` statement provides a multi-way branch. It evaluates an expression (only once)
and selects the first matching branch.

A branch is introduced by `when` and followed by a comma-separated list of comparison values
(needn't be constant). When a branch is selected, all following statements upto the next
`when` are executed and the `case` statement is left.
No `break` statement is needed to leave it, in fact, `break` would
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


## <a id="exception"></a>Exception handling
When Lox encounters a run-time error, the error message including a backtrace of
function names and line numbers is printed and the evaluation is aborted.

Lox68k allows handling errors with the `handle` expression. The first sub-expression is evaluated
in a context where any exception raised calls the error handler (the other sub-expression )
with the error message as its single argument.
Like in the `if` expression, a colon must be written between the expressions to
emphasize the special evaluation order.

```javascript
  5/0                 ⚠ "div by zero." // exception raised
  handle(5/0 : upper) → "DIV BY ZERO." // exception handled, converting message to uppercase
```
Exceptions caused by system errors have always type `string`, which is the error message.
Grep Lox68k sources for `runtimeError` to see all possible system errors.

One can also manually raise an exception with the `error` native whose argument can have any
type. This argument is passed unchanged to an active handler.

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
  var even, odd; // forward declaration due to mutual recursion
  odd  = fun (n) -> if(n==0 : error(false) : even(n-1));
  even = fun (n) -> if(n==0 : error(true)  : odd(n-1));
  return fun (n) -> handle(even(n) : fun (exc) -> if(type(exc)=="bool" : exc : error(exc)));
}

var even = makeEven();

// You may want to trace call frames by setting dbg_call(true) here

even(8)      → true  // from deep call via exception, where bools are handled and returned. 
even(9)      → false // same.
even(1000)   ⚠ "Lox call stack overflow."             // System error, re-raised in handler
even("bla")  ⚠ "Can't subtract types string and int." // same.
```


## <a id="dynvar"></a>Dynamic variables
are inspired by Common Lisp's special variables:
* Lexical variables have **lexical scope**, they are visible in certain program regions only,
  determined  at compile time, but exist for **indefinite extent** during run time 
  (even when the defining scope has been left, so-called *upvalues*)
* Dynamic variables have **indefinite scope**, they are visible anywhere in the program,
  but exist only for a limited time period (**dynamic extent**) during run time

A Lox variable is resolved lexically when it is declared locally or in an enclosing function.
If it isn't found at compile time, it is assumed to be a global variable, which is resolved
dynamically at runtime. To define a global variable in Lox, a `var` statement must be
executed at top level in a script. Global variables exist forever until the REPL is terminated.

Lox68k extends this scheme, a dynamic variable can be defined in a `dynvar` expression and
then exists during evaluation of the following subexpression, automatically being removed afterwards.
When there already is a global variable with the same name, the new value temporarily shadows
the original during evaluation. Of course those shadowings may be nested.

```javascript
  // Physics: Compute altitude in free fall.
  fun s(t) -> 0.5 * g * t * t                         // t is lexical, g is dynamic
  s(3)                  ⚠ "Undefined variable 'g'."   // Acceleration 'g' not defined, yet
  var g = 9.81;                                       // Make Earth's acceleration the default.
  s(3)                  → 44.145                      // Altitude after 3s fall.
  dynvar(g=1.62 : s(3)) →  7.29                       // Temporarily falling on the moon.
  s(3)                  → 44.145                      // We are back on earth.
```
Another application of dynamic variables is instrumentation of code:

```javascript
  // define a wrapper to log function calls
  fun logging(f) -> 
    fun(..args) {                                // variadic function
      print "Calling ", f, " with args ", args,; // trailing ',' suppresses
                                                 //   linefeed after printing
      var res = f(..args);                       // call wrapped function
                                                 //   with all original arguments
      print " gives ", res;                      // continue printing on same line
      return res;
    }

  fun hypot(a,b) -> sqrt(pow(a,2)+pow(b,2)) // 'pow' is a native function, defined globally

  // Rebind 'pow' to its logging version during testing 
  fun test(a,b) -> dynvar(pow=logging(pow) : hypot(a,b)) 

  test(5,12) → 13.0
    ✎ Calling <native pow> with args [5, 2] gives 25.0
    ✎ Calling <native pow> with args [12, 2] gives 144.0
             
```
Lexical variables are the default and usually better than dynamic variables, but dynamic variables
are occasionally useful and elegant. 


## <a id="debug"></a>Debugging utilities
All debugging utilities are only available when Lox68k is compiled with the symbol `LOX_DBG`
defined. Then they include the ones given in the book and a few more, but all debugging options
are separately switchable by calling the corresponding function with a truish or falsey argument.

### Bytecode disassembler (*book*)
Control disassembling the generated byte code after compilation with the switch
`dbg_code(arg)`.

### Execution statistics (*new*)
Control printing execution statistic after an evaluation with the switch
`dbg_stat(arg)`. Printed are
  * real time used 
  * number of virtual machine instructions 
  * number of bytes allocated
  * number of garbage collections

### <a id="trace"></a>Tracing calls (*new*)
Control tracing of calls to closures with the switch `dbg_call(arg)`. Each call
is logged with all arguments in a single line, which is prefixed by `-->` and indented according
to the nesting depth.

On return, `<--` and the returned value are printed, also indented.

To control tracing of calls to native functions, use the switch `dbg_nat(arg)`.
Arguments and results of every call are printed in a single line,
starting with `---`, only indented when also tracing closure calls.

Summary of tracing indicators:
  * `-->` calling a closure
  * `<--` returning from a closure call
  * `==>` establishing an exception handler
  * `<==` an exception is raised
  * `~~>` binding a dynamic variable
  * `---` calling a native function, `->` returning result or `/!\` raising exception

### Tracing VM steps (*book*)
Control tracing every VM step with the switch `dbg_step(arg)`.
This creates huge amount of output.

### Tracing garbage collection (*book/extended*)
`dbg_gc(bits)` in contrast expects a bit mask (see `memory.h` for details) to select logging
several garbage collector messages. This may also create lots of output.
 

## Equality vs. Identity
The original Lox `number` type has been split into `int` and `real`, since the 68k chip only
supports hardware integers and floating point numbers have to be emulated in software which is
quite slow.

Logically, the types `nil`, `bool`, `int`, `real`, `string`, and `native` are value types,
they have no identity and can't be modified. All other types (`list`, `closure`, `class`, `instance`,
`bound`, `iterator`) are reference types with an identity and are modifiable.

The `==` operator compares value types via their values and reference types via their identity,
which is actually a fast 32 bit comparison (`valuesEqual()` macro), also used for
`case` branch selection and instance keys.

Implementation-wise, natives, strings and reals are objects in the heap, but
behave like value types, since:
  * Natives are constants (pointers into ROM), there's no way to create a new native from Lox.
  * Strings are interned, so a duplicate string is in fact the *identical* object.
  * Reals are not interned, so `1.2 == 1.2` is `false` (two distinct objects),
    but **reals shouldn't be compared for equality anyway**. And don't use reals as instance
    keys or `case` branch selectors for the same reason.

However, if you really want to compare reals for numeric equality, you can use the `equal`
native, which also does type conversion, so `equal(3, 3.0)` is `true`.

## Minor extensions

### Numeric literals

#### <a id="hex"></a>Hexadecimal
By prefixing `$`, an integer can be written in hexadecimal. Both lower and upper letters are
allowed.
```javascript
  $AB       → 171
  $fffffffe → -2
  $3fffffff → 1073741823  // maxint (31 bits)
  $40000000 → -1073741824 // minint (31 bits)
  hex(255)  → "ff"
```

#### <a id="bin"></a>Binary
A binary literal is introduced by `%` and includes all `0` and `1` chars thereafter.
```javascript
  %11111    → 31
  %101      → 5
  bin(255)  → "11111111"
  %         ✪ "No digits after radix."
  poke($f0000, %10101010); // Sets corresponding debug LEDs on 68008 Kit
```

#### Exponents
Real numbers can have a decimal exponent in a standard way
```javascript
  1e3       → 1000.0 // always printed with a decimal point to distinguish reals from ints
  4.56e-5   → 4.56e-005
  23e       ✪ "Empty exponent in number."
```

### <a id="print"></a>Printing
`print` remains a statement, not a native function, so some syntactic variants are possible.
`print` allows any number of expressions to be printed; if they are separated by a single comma,
their values are printed without space, if separated by two commas, 3 spaces are inserted between.

Strings (and strings contained in subobjects) are printed without quotes.
All values are printed in a single line. If the list ends with a comma, the newline
is suppressed and the next `print` statement continues at the same line.

All Lox68k values are printable, though some types print within `< >`.

Instances print their slots, but only one level deep to avoid infinite recursion.

`print` may be abbreviated by `?`

```javascript
  print "Result is",7;
    ✎ Result is7
  print "Result is",,7,,11;
    ✎ Result is   7   11
  ? 42;
    ✎ 42
  print sqrt,,Set,,fun(){},,Set().clone,,slots(Set(14));
    ✎ <native sqrt>   <class Set>   <closure #11>   <bound Object.clone>   <iterator 1>

  var infinite = Set(38);
  infinite.self = infinite;        // We include ourselves now..
  ?infinite;                       // but can be printed nonetheless
    ✎ Set(38,true, self,Set(..))
```

### Declaring multiple variables
Multiple variables can be declared and initialized in a single `var` declaration.
```javascript
  var a=5, b=2+a, c, d=14-b; // Later variables may refer to earlier ones.
```

### <a id="interrupt"></a>Interrupting evaluation
You can interrupt an evaluation and return to the REPL prompt. This works differently 
and is described [here for each target platform](lox68k.md#varieties).


### <a id="toplevel"></a>Top-level expressions
The Lox REPL expects declarations/statements. In Lox68k, you can also enter an *expression*
at the top-level, which is evaluated *and printed*.
Syntactic ambiguities (`if` and `fun` may both start a declaration and an expression)
are resolved in favour of a declaration, but you can enclose your input in `(` `)`
to force interpreting it as an expression.
```javascript
  // ambiguous, parsed as a statement, leading to syntax error
  if (3>1 : "bigger" : "smaller")  
    ✪ Error at ':': Expect ')' after condition.

  // forced to be parsed as an expression, printing its value, since top-level
  (if (3>1 : "bigger" : "smaller")) → "bigger"

  // trailing ';' makes it an expression statement, nothing printed
  (if (3>1 : "bigger" : "smaller"));            

  // expressions not on top-level are not allowed
  if (3>1) "bigger" else "smaller"
    ✪ Error at 'else': Expect ';' after expression.

  // syntactically fine now, but expression statements don't print anything
  if (3>1) "bigger"; else "smaller";

  // so print explicitly in each branch
  if (3>1) print "bigger"; else print "smaller";
    ✎ bigger 

```

