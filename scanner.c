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
again:
    c = peek();
    if (c == CHAR_RS || c == CHAR_LF) {
        scanner.line++;
        goto advance;
    } else if (c == ' ' || c == CHAR_CR || c == CHAR_HT) {
        advance: advance();
        goto again;
    } else if (c == '/') {
        if (peekNext() == '/') {
            // A comment goes until the end of the line.
            while ((c = peek()) != '\0' && c != CHAR_LF && c != CHAR_RS)
                advance();
            goto again;
        }
    }
}

// Wrapping 4 bytes Trie info into a single 32 bit argument
#ifdef WRAP_BIG_ENDIAN
#define WRAP(a,b,c,d) ((a)<<24|(b)<<16|(c)<<8|(d))
#else
#define WRAP(a,b,c,d) ((a)|(b)<<8|(c)<<16|(d)<<24)
#endif
#define AMBIGUOUS (-1)

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

static Token identifier(int32_t trie) {
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
    int16_t    id_length;
    const char *src;
    char       c         = scanner.start[0];
    TokenType  tokenType = TOKEN_IDENTIFIER; 

    while (isalnum(peek()) || peek() == '_')
        advance();

    id_length = scanner.current - scanner.start;

    // Disambiguate possible keywords starting with same letter.
    if (trie == AMBIGUOUS) {
        trie = 0;
        if (id_length > 1) {   
            if (c == 'c') {
                c = scanner.start[1];
                if      (c == 'a')     trie = WRAP(2, 2, 14, TOKEN_CASE);
                else if (c == 'l')     trie = WRAP(2, 3,  9, TOKEN_CLASS);
            } else if (c == 'f') {
                c = scanner.start[1];
                if      (c == 'a')     trie = WRAP(2, 3, 13, TOKEN_FALSE);
                else if (c == 'o')     trie = WRAP(2, 1,  7, TOKEN_FOR);
                else if (c == 'u')     trie = WRAP(2, 1,  1, TOKEN_FUN);
            } else if (c == 't') {
                c = scanner.start[1];
                if      (c == 'h')     trie = WRAP(2, 2, 29, TOKEN_THIS);
                else if (c == 'r')     trie = WRAP(2, 2, 31, TOKEN_TRUE);
            } else { // if (c == 'w') always true
                if (scanner.start[1] == 'h' && id_length > 2) {
                    c = scanner.start[2];
                    if      (c == 'e') trie = WRAP(3, 1,  1, TOKEN_WHEN);
                    else if (c == 'i') trie = WRAP(3, 2,  3, TOKEN_WHILE);
                }
            }
        }
    }

    if (trie && id_length == TRIE(start) + TRIE(length)) {
        src   = scanner.start + TRIE(start);
        rest += TRIE(offset);
        do {
            if (*src++ != *rest++)
                goto noKeyword;
        } while (--TRIE(length));
        tokenType = TRIE(type);
    }
noKeyword:
    return makeToken(tokenType);
}

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

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
    int32_t   trie = 0;

    skipWhitespace();
    scanner.start = scanner.current;

    if (isAtEnd())
        return makeToken(TOKEN_EOF);

    c = *scanner.current++;

    switch (c) {
        case '0': case '1': case '2': case '3': case '4':
        case '5': case '6': case '7': case '8': case '9':
        case '$': case '%':
            return number(c);

        // can't start keywords
        case 'd': case 'g': case 'j': case 'k': case 'l':
        case 'm': case 'q': case 'u': case 'x': case 'y':
        case 'A': case 'B': case 'C': case 'D': case 'E':
        case 'F': case 'G': case 'H': case 'I': case 'J':
        case 'K': case 'L': case 'M': case 'N': case 'O':
        case 'P': case 'Q': case 'R': case 'S': case 'T':
        case 'U': case 'V': case 'W': case 'X': case 'Y':
        case 'Z': case '_': case 'z':
        ident: 
            return identifier(trie);

        // possibly starting unique keyword
        case 'a': trie = WRAP(1, 2,  1, TOKEN_AND);    goto ident;
        case 'b': trie = WRAP(1, 4, 25, TOKEN_BREAK);  goto ident;
        case 'e': trie = WRAP(1, 3, 13, TOKEN_ELSE);   goto ident;
        case 'h': trie = WRAP(1, 5,  0, TOKEN_HANDLE); goto ident;
        case 'i': trie = WRAP(1, 1, 16, TOKEN_IF);     goto ident;
        case 'n': trie = WRAP(1, 2, 12, TOKEN_NIL);    goto ident;
        case 'o': trie = WRAP(1, 1,  7, TOKEN_OR);     goto ident;
        case 'p': trie = WRAP(1, 4, 18, TOKEN_PRINT);  goto ident; 
        case 'r': trie = WRAP(1, 5,  4, TOKEN_RETURN); goto ident;
        case 's': trie = WRAP(1, 4, 22, TOKEN_SUPER);  goto ident;
        case 'v': trie = WRAP(1, 2, 17, TOKEN_VAR);    goto ident;

        // possibly starting several keywords
        case 'c': case 'f': case 't': case 'w':
            trie = AMBIGUOUS;
            goto ident; 

        case '"':
            return string();

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
