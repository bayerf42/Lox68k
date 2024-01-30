# Grammar for the Lox68k port

Changes to [original Lox grammar](https://craftinginterpreters.com/appendix-i.html) are:
* `print` can be abbreviated by `?` and allows a list of expressions, printed on one line
* Expressions in `print` may be separated by two commas, inserting some spaces into the output.
* Ending a `print` list with one or two commas suppresses printing the linefeed.
* `[` ... `]` syntax for building lists
* `[ exp ]` as string, list, or instance index to access elements
* `[ exp? : exp? ]` as list or string slice to extract subsequence
* operator `\` for division remainder
* `$` prefix for hexadecimal integer literals
* `%` prefix for binary integer literals
* anonymous functions as expressions `fun (params*) {block}`
* prefix `..` before last parameter name for rest parameter in function declaration
* prefix `..` before argument to unpack list into arguments in function call
* Real numbers with exponential part
* postfix `@` to read key part of a hashtable iterator entry
* postfix `^` to read/write value part of a hashtable iterator entry
* function body `{ return expr;}` can be abbreviated by `-> expr`.
* `case` statement for multiway branch


``` ebnf
program        → declaration* EOF ;

declaration    → class_decl
               | fun_decl
               | var_decl
               | statement ;

class_decl     → "class" IDENTIFIER ( "<" IDENTIFIER )?
                 "{" ( IDENTIFIER function )* "}" ;
fun_decl       → "fun" IDENTIFIER function ;
var_decl       → "var" sing_var_decl ( "," sing_var_decl )* ";" ;
sing_var_decl  → IDENTIFIER ( "=" expression )? ;

statement      → expr_stmt
               | for_stmt
               | if_stmt
               | print_stmt
               | return_stmt
               | while_stmt
               | case_stmt
               | block ;

expr_stmt      → expression ";" ;
for_stmt       → "for" "(" ( var_decl | expr_stmt | ";" )
                           expression? ";"
                           expression? ")" statement ;
if_stmt        → "if" "(" expression ")" statement ( "else" statement )? ;
print_stmt     → ("print" | "?") print_list ";" ;
return_stmt    → "return" expression? ";" ;
while_stmt     → "while" "(" expression ")" statement ;
case_stmt      → "case" "(" expression ")" "{" when_branch* else_branch? "}" ;
when_branch    → "when" expression ( "," expression )* ":" statement+ ;
else_branch    → "else" statement+ ;
block          → "{" declaration* "}" ;

expression     → assignment ;

assignment     → postfix "=" assignment | logic_or ;

logic_or       → logic_and ( "or" logic_and )* ;
logic_and      → equality ( "and" equality )* ;
equality       → comparison ( ( "!=" | "==" ) comparison )* ;
comparison     → term ( ( ">" | ">=" | "<" | "<=" ) term )* ;
term           → factor ( ( "-" | "+" ) factor )* ;
factor         → unary ( ( "/" | "*" | "\") unary )* ;

unary          → ( "!" | "-" ) unary | postfix ;
postfix        → primary ( "(" arguments ")" | "." IDENTIFIER |
                           "[" ( index | slice ) "]" | "@" | "^" )* ;
index          → expression ;
slice          → expression ":" expression | expression ":" | ":" expression | ":" ;
primary        → "true" | "false" | "nil" | "this"
               | NUMBER | STRING | IDENTIFIER | "(" expression ")"
               | "[" arguments "]" | "super" "." IDENTIFIER
               | "fun" function ;

function       → "(" parameters ")" (block | "->" expression) ;
parameters     → ( rest_parameter | IDENTIFIER ( "," IDENTIFIER )* ( "," rest_parameter )? )? ;
rest_parameter → ".." IDENTIFIER ;
arguments      → ( argument ( "," argument )* )? ;
argument       → ".."? expression ;
print_list     → ( expression ( print_sep expression )* print_sep? )? ;
print_sep      → "," ","? ;

NUMBER         → DIGIT+ ( "." DIGIT+ )? ( ( "e" | "E" ) ( "+" | "-" )? DIGIT+ )?
               | "$" HEXDIGIT+
               | "%" BINDIGIT+ ;
 
STRING         → "\"" <any char except "\"">* "\"" ;
IDENTIFIER     → ALPHA ( ALPHA | DIGIT )* ;
ALPHA          → "a" ... "z" | "A" ... "Z" | "_" ;
DIGIT          → "0" ... "9" ;
HEXDIGIT       → "0" ... "9" | "a" ... "f" | "A" ... "F" ;
BINDIGIT       → "0" ... "1" ;
```
## Characters

* `!` logical not, `!=` not equal
* `"` begin string literal, end string literal
* `#` illegal
* `$` begin hexadecimal number literal
* `%` begin binary number literal
* `&` illegal on Kit, load a file on Windows/Linux
* `'` illegal
* `(` begin grouping, begin call
* `)` end grouping, end call
* `*` multiplication
* `+` addition
* `,` list separator
* `-` subtraction, negation, `->` return shorthand
* `.` decimal point, property access, `..` unpack list, rest argument
* `/` division, `//` comment until end of line
* `0-9` digit
* `:` case branch separator, splice separator
* `;` statement terminator
* `<` less than, subclass, `<=` less or equal
* `=` assignment, `==` equals
* `>` greater than, `>=` greater or equal
* `?` shorthand for `print`
* `@` key of iterator
* `A-Z` letter
* `[` begin list, begin subscript
* `\` modulo operator
* `]` end list, end subscript
* `^` value of iterator
* `_` letter
* `` ` `` illegal
* `a-z` letter
* `{` begin code block
* `|` illegal
* `}` end code block
* `~` illegal
