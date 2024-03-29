#ifndef clox_scanner_h
#define clox_scanner_h

#include "machine.h"

typedef enum {
    // single character tokens.
    TOKEN_LEFT_PAREN, TOKEN_RIGHT_PAREN,
    TOKEN_LEFT_BRACE, TOKEN_RIGHT_BRACE,
    TOKEN_LEFT_BRACKET, TOKEN_RIGHT_BRACKET,
    TOKEN_COMMA, TOKEN_DOT, TOKEN_MINUS, TOKEN_PLUS,
    TOKEN_SEMICOLON, TOKEN_COLON, TOKEN_SLASH, TOKEN_STAR, TOKEN_BACKSLASH,
    TOKEN_AT, TOKEN_HAT,
    // one or two character tokens.
    TOKEN_BANG, TOKEN_BANG_EQUAL,
    TOKEN_EQUAL, TOKEN_EQUAL_EQUAL,
    TOKEN_GREATER, TOKEN_GREATER_EQUAL,
    TOKEN_LESS, TOKEN_LESS_EQUAL,
    TOKEN_DOT_DOT, TOKEN_ARROW,
    // Literals.
    TOKEN_IDENTIFIER, TOKEN_STRING, TOKEN_INT, TOKEN_REAL,
    // KEYWORDS
    TOKEN_AND, TOKEN_BREAK, TOKEN_CASE, TOKEN_CLASS, TOKEN_ELSE, TOKEN_FALSE,
    TOKEN_FOR, TOKEN_FUN, TOKEN_HANDLE, TOKEN_IF, TOKEN_NIL, TOKEN_OR,
    TOKEN_PRINT, TOKEN_RETURN, TOKEN_SUPER, TOKEN_THIS,
    TOKEN_TRUE, TOKEN_VAR, TOKEN_WHEN, TOKEN_WHILE,

    TOKEN_ERROR, TOKEN_EOF
} TokenType;

// For printing syntax errors in consumeExp()
#define TOKEN_CHARS "(){}[],.-+;:"

// The IDE68K compiler has a bug when returning structs by value, which are smaller
// than 3 longs. In this case, very strange FMOVE instructions are generated, which
// trap on the 68008 of course. So the Token struct must be at least 12 bytes long,
// which is somewhat wasteful... 

typedef struct {
    const char* start;
    TokenType   type;
    int16_t     length;
    int32_t     line; 
} Token;

void  initScanner(const char* source);
Token scanToken(void);

#endif