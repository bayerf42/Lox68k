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

static Scanner scanner;

void initScanner(const char* source) {
    scanner.start   = source;
    scanner.current = source;
    scanner.line    = 1;
}

#define isAtEnd() (*scanner.current == '\0')
#define advance()   scanner.current++
#define peek()    (*scanner.current)

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
    token.length = scanner.current - scanner.start;
    token.line   = scanner.line;
    return token;
}

static Token errorToken(const char* message) {
    Token token;
    token.type   = TOKEN_ERROR;
    token.start  = message;
    token.length = strlen(message);
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

// Wrapping 4 bytes Trie info into a single 32 bit argument
#ifdef WRAP_BIG_ENDIAN
#define WRAP(a,b,c,d) ((a)<<24|(b)<<16|(c)<<8|(d))
#else
#define WRAP(a,b,c,d) ((a)|(b)<<8|(c)<<16|(d)<<24)
#endif

typedef struct {
    int8_t start;
    int8_t length;
    int8_t offset;
    int8_t type;
} TrieInfo;

// and accessing Trie info components
#define TRIE(name) ((TrieInfo*)&trie)->name

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wstrict-aliasing"
#endif

static TokenType identifierType(void) {
    // Compressed keyword postfixes in single string
    const char* rest =
              "andleturnassilsefarintupereakisue";
    //         012345678901234567890123456789012
    // And      ##
    // Break                            ####
    // CAse                  ##
    // CLass            ###
    // Else                 ###               
    // FAlse                ###               
    // FOr            #
    // FUn      #
    // Handle  #####
    // If                      #
    // Nil                 ##
    // Or             #
    // Print                     ####
    // Return      #####
    // Super                         ####             
    // THis                                 ##
    // TRue                                   ##
    // Var                      ##
    // WHEn     #
    // WHIle      ##
    //         012345678901234567890123456789012
    int16_t     id_length = scanner.current - scanner.start;
    int32_t     trie      = 0;
    char        c         = scanner.start[0];
    const char *src;

    if (c & LOWER_CASE_MASK) { // only lower-case names can be keywords
        // Use a hard-coded trie to recognize keywords
        // bisect on first char to reduce number of sequential comparisons
        if (c <= 'n') {
            if      (c == 'a')                 trie = WRAP(1, 2,  1, TOKEN_AND);
            else if (c == 'b')                 trie = WRAP(1, 3, 25, TOKEN_BREAK);
            else if (c == 'c') {
                if (id_length > 1) {
                    c = scanner.start[1];
                    if      (c == 'a')         trie = WRAP(2, 2, 14, TOKEN_CASE);
                    else if (c == 'l')         trie = WRAP(2, 3,  9, TOKEN_CLASS);
                }
            }
            else if (c == 'e')                 trie = WRAP(1, 3, 13, TOKEN_ELSE);
            else if (c == 'f') {
                if (id_length > 1) {
                    c = scanner.start[1];
                    if      (c == 'a')         trie = WRAP(2, 3, 13, TOKEN_FALSE);
                    else if (c == 'o')         trie = WRAP(2, 1,  7, TOKEN_FOR);
                    else if (c == 'u')         trie = WRAP(2, 1,  1, TOKEN_FUN);
                }
            }
            else if (c == 'h')                 trie = WRAP(1, 5,  0, TOKEN_HANDLE);
            else if (c == 'i')                 trie = WRAP(1, 1, 16, TOKEN_IF);
            else if (c == 'n')                 trie = WRAP(1, 2, 12, TOKEN_NIL);
        } else {  // c >  'n' 
            if      (c == 'o')                 trie = WRAP(1, 1,  7, TOKEN_OR);
            else if (c == 'p')                 trie = WRAP(1, 4, 18, TOKEN_PRINT);
            else if (c == 'r')                 trie = WRAP(1, 5,  4, TOKEN_RETURN);
            else if (c == 's')                 trie = WRAP(1, 4, 22, TOKEN_SUPER);
            else if (c == 't') {
                if (id_length > 1) {
                    c = scanner.start[1];
                    if      (c == 'h')         trie = WRAP(2, 2, 29, TOKEN_THIS);
                    else if (c == 'r')         trie = WRAP(2, 2, 31, TOKEN_TRUE);
                }
            }
            else if (c == 'v')                 trie = WRAP(1, 2, 17, TOKEN_VAR);
            else if (c == 'w') {
                if (id_length > 1 &&
                    scanner.start[1] == 'h' &&
                    id_length > 2) {
                    c = scanner.start[2];
                    if      (c == 'e')         trie = WRAP(3, 1,  1, TOKEN_WHEN);
                    else if (c == 'i')         trie = WRAP(3, 2,  3, TOKEN_WHILE);
                }
            }
        }
    }

    if (trie && scanner.current - scanner.start == TRIE(start) + TRIE(length)) {
        src   = scanner.start + TRIE(start);
        rest += TRIE(offset);
        do {
            if (*src++ != *rest++)
                return TOKEN_IDENTIFIER;
        } while (--TRIE(length));
        return TRIE(type);
    }
    return TOKEN_IDENTIFIER;
}

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

static Token identifier(void) {
    while (isalnum(peek()) || peek() == '_')
        advance();
    return makeToken(identifierType());
}

static Token number(char start) {
    bool exponentEmpty = true;
    bool isReal        = false;

    if (start == '%')
        while ((peek() | 0x01) == '1') // '0' or '1'
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
        if ((peek() | LOWER_CASE_MASK) == 'e') { // 'E' or 'e'
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
    char      c;
    TokenType token;

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
        case '"': return string();
        case '(': token = TOKEN_LEFT_PAREN;    break;
        case ')': token = TOKEN_RIGHT_PAREN;   break;
        case '{': token = TOKEN_LEFT_BRACE;    break;
        case '}': token = TOKEN_RIGHT_BRACE;   break;
        case '[': token = TOKEN_LEFT_BRACKET;  break;
        case ']': token = TOKEN_RIGHT_BRACKET; break;
        case ';': token = TOKEN_SEMICOLON;     break;
        case ':': token = TOKEN_COLON;         break;
        case ',': token = TOKEN_COMMA;         break;
        case '+': token = TOKEN_PLUS;          break;
        case '/': token = TOKEN_SLASH;         break;
        case '*': token = TOKEN_STAR;          break;
        case 92 : token = TOKEN_BACKSLASH;     break; // IDE68k doesn't like '\\'
        case '@': token = TOKEN_AT;            break;
        case '^': token = TOKEN_HAT;           break;
        case '?': token = TOKEN_PRINT;         break;
        case '!': if (match('=')) token = TOKEN_BANG_EQUAL;    else token = TOKEN_BANG;    break;
        case '=': if (match('=')) token = TOKEN_EQUAL_EQUAL;   else token = TOKEN_EQUAL;   break;
        case '<': if (match('=')) token = TOKEN_LESS_EQUAL;    else token = TOKEN_LESS;    break;
        case '>': if (match('=')) token = TOKEN_GREATER_EQUAL; else token = TOKEN_GREATER; break;
        case '.': if (match('.')) token = TOKEN_DOT_DOT;       else token = TOKEN_DOT;     break;
        case '-': if (match('>')) token = TOKEN_ARROW;         else token = TOKEN_MINUS;   break;
        default:
            sprintf(buffer, "Invalid character '%c'.", c);
            return errorToken(buffer);
    }
    return makeToken(token);
}
