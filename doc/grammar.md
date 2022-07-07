# Grammar for the Lox68k port

Changes to [original Lox grammar](https://craftinginterpreters.com/appendix-i.html) are:
* `print` can be abbreviated by `?` and allows a list of expressions, printed on one line
* `[` ... `]` syntax for list expressions
* `[ exp ]` as string, list, or instance index to access elements
* `[ exp? : exp? ]` as list or string slice to extract subsequence
* operator `%` for division remainder
* no fractional part in integer literals
* `$` prefix for hexadecimal integer literals


``` ebnf
program        → declaration* EOF ;

declaration    → classDecl
               | funDecl
               | varDecl
               | statement ;

classDecl      → "class" IDENTIFIER ( "<" IDENTIFIER )?
                 "{" function* "}" ;
funDecl        → "fun" function ;
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
printStmt      → ("print" | "?") ( arguments ","? )? ";" ;
returnStmt     → "return" expression? ";" ;
whileStmt      → "while" "(" expression ")" statement ;
block          → "{" declaration* "}" ;

expression     → assignment ;

assignment     → ( call "." )? IDENTIFIER "=" assignment
               | call "[" index "]" "=" assignment
               | logic_or ;

logic_or       → logic_and ( "or" logic_and )* ;
logic_and      → equality ( "and" equality )* ;
equality       → comparison ( ( "!=" | "==" ) comparison )* ;
comparison     → term ( ( ">" | ">=" | "<" | "<=" ) term )* ;
term           → factor ( ( "-" | "+" ) factor )* ;
factor         → unary ( ( "/" | "*" | "%") unary )* ;

unary          → ( "!" | "-" ) unary | call ;
call           → subscript ( "(" arguments? ")" | "." IDENTIFIER )* ;
subscript      → primary ( "[" index | slice "]" )* ;
index          → expression ;
slice          → expression ":" expression | expression ":" | ":" expression | ":" ;
primary        → "true" | "false" | "nil" | "this"
               | NUMBER | STRING | IDENTIFIER | "(" expression ")"
               | "[" arguments? "]" | "super" "." IDENTIFIER ;

function       → IDENTIFIER "(" parameters? ")" block ;
parameters     → IDENTIFIER ( "," IDENTIFIER )* ;
arguments      → expression ( "," expression )* ;

NUMBER         → DIGIT+ | "$" HEXDIGIT+ ;
STRING         → "\"" <any char except "\"">* "\"" ;
IDENTIFIER     → ALPHA ( ALPHA | DIGIT )* ;
ALPHA          → "a" ... "z" | "A" ... "Z" | "_" ;
DIGIT          → "0" ... "9" ;
HEXDIGIT       → "0" ... "9" | "a" ... "f" | "A" ... "F" ;

```