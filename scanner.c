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
    int16_t     line;
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

static void makeToken(Token* token, TokenType type) {
    token->type   = type;
    token->start  = scanner.start;
    token->length = scanner.current - scanner.start;
    token->line   = scanner.line;
}

static void errorToken(Token* token, const char* message) {
    token->type   = TOKEN_ERROR;
    token->start  = message;
    token->length = strlen(message);
    token->line   = scanner.line;
}

static void makeNumToken(Token* token, bool isReal) {
    makeToken(token, isReal ? TOKEN_REAL : TOKEN_INT);
    if (token->length == 1 && !isdigit(*token->start))
        errorToken(token, "No digits after radix.");
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

static void number(Token* token, char start) {
    bool exponentEmpty = true;
    bool isReal        = false;

    if (start == '%')
        while (isbinary(peek()))
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
            if (exponentEmpty) {
                errorToken(token, "Empty exponent part.");
                return;
            }
        }
    }

    // Don't silently begin a new token when an alphanumeric char follows directly
    if (isalnum(peek())) {
        sprintf(buffer, "Invalid digit '%c'.", peek());
        errorToken(token, buffer);
	return;
    }

    makeNumToken(token, isReal);
}

static void string(Token* token) {
    while (peek() != '"' && !isAtEnd()) {
        if (peek() == CHAR_LF || peek() == CHAR_RS)
            scanner.line++;
        advance();
    }

    if (isAtEnd()) {
        errorToken(token, "Unterminated string.");
        return;
    }

    // the closing quote
    advance();
    makeToken(token, TOKEN_STRING);
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

static void identifier(Token* token, int32_t trie) {
    // Overlapping keyword postfixes in single string
    const char* rest =
              "andleturnassilsefynvarintupereakisue";
    //         012345678901234567890123456789012345
    // And      ##
    // Break                               ####
    // CAse                  ##
    // CLass            ###
    // Dynvar                   #####    
    // Else                 ###               
    // FAlse                ###               
    // FOr            #
    // FUn      #
    // Handle  #####
    // If                      #
    // Nil                 ##
    // Or             #
    // Print                        ####
    // Return      #####
    // Super                            ####             
    // THis                                    ##
    // TRue                                      ##
    // Var                         ##
    // WHEn     #
    // WHIle      ##
    //         012345678901234567890123456789012345
    int16_t    id_length;
    const char *src;
    char       c     = scanner.start[0];
    char       c1;
    TokenType  tType = TOKEN_IDENTIFIER; 

    while (isalnum(peek()) || peek() == '_')
        advance();

    id_length = scanner.current - scanner.start;

    // Disambiguate possible keywords starting with same letter.
    if (trie == AMBIGUOUS) {
        trie = 0;
        if (id_length > 1) {   
            c1 = scanner.start[1];
            if (c == 'c') {
                if      (c1 == 'a')     trie = WRAP(2, 2, 14, TOKEN_CASE);
                else if (c1 == 'l')     trie = WRAP(2, 3,  9, TOKEN_CLASS);
            } else if (c == 'f') {
                if      (c1 == 'a')     trie = WRAP(2, 3, 13, TOKEN_FALSE);
                else if (c1 == 'o')     trie = WRAP(2, 1,  7, TOKEN_FOR);
                else if (c1 == 'u')     trie = WRAP(2, 1,  1, TOKEN_FUN);
            } else if (c == 't') {
                if      (c1 == 'h')     trie = WRAP(2, 2, 32, TOKEN_THIS);
                else if (c1 == 'r')     trie = WRAP(2, 2, 34, TOKEN_TRUE);
            } else { // if (c == 'w') always true
                if (c1 == 'h' && id_length > 2) {
                    c1 = scanner.start[2];
                    if      (c1 == 'e') trie = WRAP(3, 1,  1, TOKEN_WHEN);
                    else if (c1 == 'i') trie = WRAP(3, 2,  3, TOKEN_WHILE);
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
        tType = TRIE(type);
    }
noKeyword:
    makeToken(token, tType);
}

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

void scanToken(Token* token) {
    char      c;
    TokenType tType;
    int32_t   trie = 0;

    skipWhitespace();
    scanner.start = scanner.current;

    if (isAtEnd()) {
        makeToken(token, TOKEN_EOF);
        return;
    }

    c = *scanner.current++;

    switch (c) {
        case '0': case '1': case '2': case '3': case '4':
        case '5': case '6': case '7': case '8': case '9':
        case '$': case '%':
            number(token, c);
            return;

        // can't start keywords
        case 'g': case 'j': case 'k': case 'l': case 'm':
        case 'q': case 'u': case 'x': case 'y': case 'z':
        case 'A': case 'B': case 'C': case 'D': case 'E':
        case 'F': case 'G': case 'H': case 'I': case 'J':
        case 'K': case 'L': case 'M': case 'N': case 'O':
        case 'P': case 'Q': case 'R': case 'S': case 'T':
        case 'U': case 'V': case 'W': case 'X': case 'Y':
        case 'Z': case '_':
        ident: 
            identifier(token, trie);
            return;

        // possibly starting unique keyword
        case 'a': trie = WRAP(1, 2,  1, TOKEN_AND);    goto ident;
        case 'b': trie = WRAP(1, 4, 28, TOKEN_BREAK);  goto ident;
        case 'd': trie = WRAP(1, 5, 17, TOKEN_DYNVAR); goto ident;
        case 'e': trie = WRAP(1, 3, 13, TOKEN_ELSE);   goto ident;
        case 'h': trie = WRAP(1, 5,  0, TOKEN_HANDLE); goto ident;
        case 'i': trie = WRAP(1, 1, 16, TOKEN_IF);     goto ident;
        case 'n': trie = WRAP(1, 2, 12, TOKEN_NIL);    goto ident;
        case 'o': trie = WRAP(1, 1,  7, TOKEN_OR);     goto ident;
        case 'p': trie = WRAP(1, 4, 21, TOKEN_PRINT);  goto ident; 
        case 'r': trie = WRAP(1, 5,  4, TOKEN_RETURN); goto ident;
        case 's': trie = WRAP(1, 4, 25, TOKEN_SUPER);  goto ident;
        case 'v': trie = WRAP(1, 2, 20, TOKEN_VAR);    goto ident;

        // possibly starting several keywords
        case 'c': case 'f': case 't': case 'w':
            trie = AMBIGUOUS;
            goto ident; 

        case '"':
            string(token);
            return;

        case '(': tType = TOKEN_LEFT_PAREN;    break;
        case ')': tType = TOKEN_RIGHT_PAREN;   break;
        case '{': tType = TOKEN_LEFT_BRACE;    break;
        case '}': tType = TOKEN_RIGHT_BRACE;   break;
        case '[': tType = TOKEN_LEFT_BRACKET;  break;
        case ']': tType = TOKEN_RIGHT_BRACKET; break;
        case ';': tType = TOKEN_SEMICOLON;     break;
        case ':': tType = TOKEN_COLON;         break;
        case ',': tType = TOKEN_COMMA;         break;
        case '+': tType = TOKEN_PLUS;          break;
        case '/': tType = TOKEN_SLASH;         break;
        case '*': tType = TOKEN_STAR;          break;
        case 92 : tType = TOKEN_BACKSLASH;     break; // IDE68k doesn't like '\\'
        case '@': tType = TOKEN_AT;            break;
        case '^': tType = TOKEN_HAT;           break;
        case '?': tType = TOKEN_PRINT;         break;
        case '!': if (match('=')) tType = TOKEN_BANG_EQUAL;    else tType = TOKEN_BANG;    break;
        case '=': if (match('=')) tType = TOKEN_EQUAL_EQUAL;   else tType = TOKEN_EQUAL;   break;
        case '<': if (match('=')) tType = TOKEN_LESS_EQUAL;    else tType = TOKEN_LESS;    break;
        case '>': if (match('=')) tType = TOKEN_GREATER_EQUAL; else tType = TOKEN_GREATER; break;
        case '.': if (match('.')) tType = TOKEN_DOT_DOT;       else tType = TOKEN_DOT;     break;
        case '-': if (match('>')) tType = TOKEN_ARROW;         else tType = TOKEN_MINUS;   break;

        default:
            sprintf(buffer, "Invalid character '%c'.", c);
            errorToken(token, buffer);
            return;
    }
    makeToken(token, tType);
}
