#include <stdio.h>
#include <string.h>

#include "common.h"
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

static bool isAlpha(char c) {
    return c >= 'a' && c <= 'z'
        || c >= 'A' && c <= 'Z'
        || c == '_';
}

static bool isDigit(char c) {
    return c >= '0' && c <= '9';
}

static bool isHexDigit(char c) {
    return c >= '0' && c <= '9'
        || c >= 'a' && c <= 'f'
        || c >= 'A' && c <= 'F';
}

#define isAtEnd() (*scanner.current == '\0')

#define advance() (*scanner.current++)

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
    token.real   = false;
    return token;
}

static Token makeNumToken(bool isReal) {
    Token token  = makeToken(TOKEN_NUMBER);
    token.real   = isReal;
    return token;
}

static Token errorToken(const char* message) {
    Token token;
    token.type   = TOKEN_ERROR;
    token.start  = message;
    token.length = (int)strlen(message);
    token.line   = scanner.line;
    token.real   = false;
    return token;
}

static void skipWhitespace(void) {
    char c;
    for (;;) {
        c = peek();
        switch (c) {
            case ' ':
            case CHAR_CR:
            case CHAR_HT:
                advance();
                break;
            case CHAR_RS:
            case CHAR_LF:
                scanner.line++;
                advance();
                break;
            case '/':
                if (peekNext() == '/') {
                    // A comment goes until the end of the line.
                    while (peek() != CHAR_LF && peek() != CHAR_RS && !isAtEnd())
                        advance();
                    break;
                } else
                    return;
            default:
                return;
        }
    }
}

static TokenType checkKeyword(int start, int length, const char* rest, TokenType type) {
    if (scanner.current - scanner.start == start + length &&
        fix_memcmp(scanner.start + start, rest, length) == 0)
            return type;
    return TOKEN_IDENTIFIER;
}

static TokenType identifierType(void) {
    switch (scanner.start[0]) {
        case 'a': return checkKeyword(1, 2, "nd",    TOKEN_AND);
        case 'c': return checkKeyword(1, 4, "lass",  TOKEN_CLASS);
        case 'e': return checkKeyword(1, 3, "lse",   TOKEN_ELSE);
        case 'f':
            if (scanner.current - scanner.start > 1)
                switch (scanner.start[1]) {
                    case 'a' : return checkKeyword(2, 3, "lse", TOKEN_FALSE);
                    case 'o' : return checkKeyword(2, 1, "r",   TOKEN_FOR);
                    case 'u' : return checkKeyword(2, 1, "n",   TOKEN_FUN);
                }
            break;
        case 'i': return checkKeyword(1, 1, "f",     TOKEN_IF);
        case 'n': return checkKeyword(1, 2, "il",    TOKEN_NIL);
        case 'o': return checkKeyword(1, 1, "r",     TOKEN_OR);
        case 'p': return checkKeyword(1, 4, "rint",  TOKEN_PRINT);
        case 'r': return checkKeyword(1, 5, "eturn", TOKEN_RETURN);
        case 's': return checkKeyword(1, 4, "uper",  TOKEN_SUPER);
        case 't':
            if (scanner.current - scanner.start > 1)
                switch (scanner.start[1]) {
                    case 'h' : return checkKeyword(2, 2, "is", TOKEN_THIS);
                    case 'r' : return checkKeyword(2, 2, "ue", TOKEN_TRUE);
                }
            break;
        case 'v': return checkKeyword(1, 2, "ar",    TOKEN_VAR);
        case 'w': return checkKeyword(1, 4, "hile",  TOKEN_WHILE);
    }
    return TOKEN_IDENTIFIER;
}

static Token identifier(void) {
    while (isAlpha(peek()) || isDigit(peek()))
        advance();
    return makeToken(identifierType());
}

static Token number(char start) {
    bool exponentEmpty = true;
    bool isReal = false;

    if (start == '$')
        while (isHexDigit(peek()))
            advance();
    else {
        while (isDigit(peek()))
            advance();

        // Look for fractional part
        if (peek() == '.' && isDigit(peekNext())) {
            // Consume the ".".
            advance();
            isReal = true;
            while (isDigit(peek()))
                advance();
        }
        // Look for exponential part
        if (peek() == 'e' || peek() == 'E') {
            advance();
            isReal = true;
            if (peek() == '+' || peek() == '-')
                advance();
            while (isDigit(peek())) {
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

    c = advance();

    if (isAlpha(c))
        return identifier();

    if (isDigit(c) || c=='$')
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
        case '%': return makeToken(TOKEN_PERCENT);
        case '@': return makeToken(TOKEN_AT);
        case '^': return makeToken(TOKEN_HAT);
        case '!': return makeToken(match('=') ? TOKEN_BANG_EQUAL    : TOKEN_BANG);
        case '=': return makeToken(match('=') ? TOKEN_EQUAL_EQUAL   : TOKEN_EQUAL);
        case '<': return makeToken(match('=') ? TOKEN_LESS_EQUAL    : TOKEN_LESS);
        case '>': return makeToken(match('=') ? TOKEN_GREATER_EQUAL : TOKEN_GREATER);
        case '"': return string();
        case '?': return makeToken(TOKEN_PRINT);
    }

    return errorToken("Unexpected character.");
}
