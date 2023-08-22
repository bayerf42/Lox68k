#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include "memory.h"
#include "scanner.h"

#define CHAR_HT 0x09 // ASCII horizontal tab
#define CHAR_LF 0x0a // ASCII line feed
#define CHAR_CR 0x0d // ASCII carriage return
#define CHAR_RS 0x1e // ASCII record separator, used to separate input lines without triggering eval

typedef struct {
    const char* start;
    const char* current;
    int         line;
} Scanner;

Scanner scanner;

void initScanner(const char* source) {
    scanner.start   = source;
    scanner.current = source;
    scanner.line    = 1;
}

#define isAtEnd() (*scanner.current == '\0')

#define advance() scanner.current++

#define peek() (*scanner.current)

static char peekNext(void) {
    if (isAtEnd())
        return '\0';
    return scanner.current[1];
}

static bool match(char expected) {
    if (isAtEnd() || *scanner.current != expected)
        return false;
    scanner.current++;
    return true;
}

static Token makeToken(TokenType type) {
    Token token;
    token.type   = type;
    token.start  = scanner.start;
    token.length = (int16_t)(scanner.current - scanner.start);
    token.line   = scanner.line;
    return token;
}

static Token errorToken(const char* message) {
    Token token;
    token.type   = TOKEN_ERROR;
    token.start  = message;
    token.length = (int16_t)strlen(message);
    token.line   = scanner.line;
    return token;
}

static Token makeNumToken(bool isReal) {
    Token token = makeToken(isReal ? TOKEN_REAL : TOKEN_INT);
    if (token.length == 1 && !isdigit(*token.start))
        return errorToken("No digits after radix.");
    return token;
}

static void skipWhitespace(void) {
    char c;
    for (;;) {
        c = peek();
        switch (c) {
            case CHAR_RS:
            case CHAR_LF:
                scanner.line++;
                // fall thru
            case ' ':
            case CHAR_CR:
            case CHAR_HT:
                advance();
                break;

            case '/':
                if (peekNext() == '/') {
                    // A comment goes until the end of the line.
                    while (peek() != CHAR_LF && peek() != CHAR_RS && !isAtEnd())
                        advance();
                    break;
                }
                // fall thru

            default:
                return;
        }
    }
}

// Wrapping and unwrapping 4 bytes to/from a single 32 bit argument
#ifdef WRAP_BIG_ENDIAN
#define WRAP(a,b,c,d) ((a)<<24|(b)<<16|(c)<<8|(d))
#else
#define WRAP(a,b,c,d) ((a)|(b)<<8|(c)<<16|(d)<<24)
#endif
#define UNWRAP(tup,n) ((uint8_t*)&(tup))[(n)]

#define ARG_START  0
#define ARG_LENGTH 1
#define ARG_OFFSET 2
#define ARG_TYPE   3

static TokenType checkKeyword(int args) {
    // Compressed keyword postfixes in single string
    const char* rest = "ileturndasslsefarintuperisue";
    //                  0123456789012345678901234567
    //          And           ##
    //          CAse                ##
    //          CLass           ###
    //          Else               ###               
    //          FAlse              ###               
    //          FOr          #
    //          FUn           #
    //          If                    #
    //          Nil     ##
    //          Or           #
    //          Print                   ####
    //          Return    #####
    //          Super                       ####             
    //          THis                            ##
    //          TRue                              ##
    //          Var                    ##
    //          WHEn          #
    //          WHIle    ##
    const char  *src, *key;
    int         len = UNWRAP(args, ARG_LENGTH);
    if (scanner.current - scanner.start == UNWRAP(args, ARG_START) + len) {
        src = scanner.start + UNWRAP(args, ARG_START);
        key = rest + UNWRAP(args, ARG_OFFSET);
        while (len--) 
            if (*src++ != *key++)
                return TOKEN_IDENTIFIER;
        return UNWRAP(args, ARG_TYPE);
    }
    return TOKEN_IDENTIFIER;
}

static TokenType identifierType(void) {
    int id_length = scanner.current - scanner.start;
    switch (scanner.start[0]) {
        case 'a': return checkKeyword(WRAP(1, 2, 6, TOKEN_AND));
        case 'c':
            if (id_length > 1)
                switch (scanner.start[1]) {
                    case 'a' : return checkKeyword(WRAP(2, 2, 12, TOKEN_CASE));
                    case 'l' : return checkKeyword(WRAP(2, 3,  8, TOKEN_CLASS));
                }
            break;
        case 'e': return checkKeyword(WRAP(1, 3, 11, TOKEN_ELSE));
        case 'f':
            if (id_length > 1)
                switch (scanner.start[1]) {
                    case 'a' : return checkKeyword(WRAP(2, 3, 11, TOKEN_FALSE));
                    case 'o' : return checkKeyword(WRAP(2, 1,  5, TOKEN_FOR));
                    case 'u' : return checkKeyword(WRAP(2, 1,  6, TOKEN_FUN));
                }
            break;
        case 'i': return checkKeyword(WRAP(1, 1, 14, TOKEN_IF));
        case 'n': return checkKeyword(WRAP(1, 2,  0, TOKEN_NIL));
        case 'o': return checkKeyword(WRAP(1, 1,  5, TOKEN_OR));
        case 'p': return checkKeyword(WRAP(1, 4, 16, TOKEN_PRINT));
        case 'r': return checkKeyword(WRAP(1, 5,  2, TOKEN_RETURN));
        case 's': return checkKeyword(WRAP(1, 4, 20, TOKEN_SUPER));
        case 't':
            if (id_length > 1)
                switch (scanner.start[1]) {
                    case 'h' : return checkKeyword(WRAP(2, 2, 24, TOKEN_THIS));
                    case 'r' : return checkKeyword(WRAP(2, 2, 26, TOKEN_TRUE));
                }
            break;
        case 'v': return checkKeyword(WRAP(1, 2, 15, TOKEN_VAR));
        case 'w':
            if (id_length > 1 && scanner.start[1] == 'h' && id_length > 2)
                switch (scanner.start[2]) {
                    case 'e' : return checkKeyword(WRAP(3, 1, 6, TOKEN_WHEN));
                    case 'i' : return checkKeyword(WRAP(3, 2, 1, TOKEN_WHILE));
                }
            break;
    }
    return TOKEN_IDENTIFIER;
}

static Token identifier(void) {
    while (isalnum(peek()) || peek() == '_')
        advance();
    return makeToken(identifierType());
}

static Token number(char start) {
    bool exponentEmpty = true;
    bool isReal        = false;

    if (start == '%')
        while (peek() == '0' || peek() == '1')
            advance();
    else if (start == '$')
        while (isxdigit(peek()))
            advance();
    else {
        while (isdigit(peek()))
            advance();

        // Look for fractional part
        if (peek() == '.' && isdigit(peekNext())) {
            // Consume the ".".
            advance();
            isReal = true;
            while (isdigit(peek()))
                advance();
        }
        // Look for exponential part
        if (peek() == 'e' || peek() == 'E') {
            advance();
            isReal = true;
            if (peek() == '+' || peek() == '-')
                advance();
            while (isdigit(peek())) {
                advance();
                exponentEmpty = false;
            }
            if (exponentEmpty)
                return errorToken("Empty exponent in number.");
        }
    }
    return makeNumToken(isReal);
}

static Token string(void) {
    while (peek() != '"' && !isAtEnd()) {
        if (peek() == CHAR_LF || peek() == CHAR_RS)
            scanner.line++;
        advance();
    }

    if (isAtEnd())
        return errorToken("Unterminated string.");

    // the closing quote
    advance();
    return makeToken(TOKEN_STRING);
}

Token scanToken(void) {
    char c;
    skipWhitespace();
    scanner.start = scanner.current;

    if (isAtEnd())
        return makeToken(TOKEN_EOF);

    c = *scanner.current++;

    if (isalpha(c) || c == '_')
        return identifier();

    if (isdigit(c) || c == '$' || c == '%')
        return number(c);

    switch (c) {
        case '(': return makeToken(TOKEN_LEFT_PAREN);
        case ')': return makeToken(TOKEN_RIGHT_PAREN);
        case '{': return makeToken(TOKEN_LEFT_BRACE);
        case '}': return makeToken(TOKEN_RIGHT_BRACE);
        case '[': return makeToken(TOKEN_LEFT_BRACKET);
        case ']': return makeToken(TOKEN_RIGHT_BRACKET);
        case ';': return makeToken(TOKEN_SEMICOLON);
        case ':': return makeToken(TOKEN_COLON);
        case ',': return makeToken(TOKEN_COMMA);
        case '.': return makeToken(match('.') ? TOKEN_DOT_DOT       : TOKEN_DOT);
        case '-': return makeToken(match('>') ? TOKEN_ARROW         : TOKEN_MINUS);
        case '+': return makeToken(TOKEN_PLUS);
        case '/': return makeToken(TOKEN_SLASH);
        case '*': return makeToken(TOKEN_STAR);
        case 92 : return makeToken(TOKEN_BACKSLASH); // IDE68k doesn't like '\\'
        case '@': return makeToken(TOKEN_AT);
        case '^': return makeToken(TOKEN_HAT);
        case '!': return makeToken(match('=') ? TOKEN_BANG_EQUAL    : TOKEN_BANG);
        case '=': return makeToken(match('=') ? TOKEN_EQUAL_EQUAL   : TOKEN_EQUAL);
        case '<': return makeToken(match('=') ? TOKEN_LESS_EQUAL    : TOKEN_LESS);
        case '>': return makeToken(match('=') ? TOKEN_GREATER_EQUAL : TOKEN_GREATER);
        case '"': return string();
        case '?': return makeToken(TOKEN_PRINT);
    }

    return errorToken("Invalid character.");
}
