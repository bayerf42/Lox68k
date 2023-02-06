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
* prefix `..` before last parameter name for vararg parameter in function declaration
* prefix `..` before argument to unpack list into arguments in function call
* Real numbers with exponential part
* postfix `@` to read key part of a hashtable iterator entry
* postfix `^` to read/write value part of a hashtable iterator entry
* function body `{ return expr;}` can be abbreviated by `-> expr`. 


``` ebnf
program        → declaration* EOF ;

declaration    → classDecl
               | funDecl
               | varDecl
               | statement ;

classDecl      → "class" IDENTIFIER ( "<" IDENTIFIER )?
                 "{" ( IDENTIFIER function )* "}" ;
funDecl        → "fun" IDENTIFIER function ;
varDecl        → "var" IDENTIFIER ( "=" expression )? ";" ;

statement      → exprStmt
               | forStmt
               | ifStmt
               | printStmt
               | returnStmt
               | whileStmt
               | block ;

exprStmt       → expression ";" ;
forStmt        → "for" "(" ( varDecl | exprStmt | ";" )
                           expression? ";"
                           expression? ")" statement ;
ifStmt         → "if" "(" expression ")" statement
                 ( "else" statement )? ;
printStmt      → ("print" | "?") ( (expression ( "," ","? expression )* )? ","? ","? )?  ";" ;
returnStmt     → "return" expression? ";" ;
whileStmt      → "while" "(" expression ")" statement ;
block          → "{" declaration* "}" ;

expression     → assignment ;

assignment     → ( call "." )? IDENTIFIER "=" assignment
               | call "[" index "]" "=" assignment
               | call "^" "=" assignment
               | logic_or ;

logic_or       → logic_and ( "or" logic_and )* ;
logic_and      → equality ( "and" equality )* ;
equality       → comparison ( ( "!=" | "==" ) comparison )* ;
comparison     → term ( ( ">" | ">=" | "<" | "<=" ) term )* ;
term           → factor ( ( "-" | "+" ) factor )* ;
factor         → unary ( ( "/" | "*" | "\") unary )* ;

unary          → ( "!" | "-" ) unary | call ;
call           → subscript ( "(" arguments? ")" | "." IDENTIFIER )* ;
subscript      → primary ( ( "[" index | slice "]") | "^" | "@" )* ;
index          → expression ;
slice          → expression ":" expression | expression ":" | ":" expression | ":" ;
primary        → "true" | "false" | "nil" | "this"
               | NUMBER | STRING | IDENTIFIER | "(" expression ")"
               | "[" arguments? "]" | "super" "." IDENTIFIER
               | "fun" function ;

function       → "(" parameters? ")" (block | "->" expression) ;
parameters     → ".."? IDENTIFIER | IDENTIFIER ( "," IDENTIFIER )* ( "," ".." IDENTIFIER )? ;
arguments      → ".."? expression ( "," ".."? expression )* ;

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