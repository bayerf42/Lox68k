#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "nano_malloc.h"
#include "native.h"
#include "memory.h"
#include "vm.h"

#define VERSION "V1.3"

#ifdef KIT68K

#include "monitor4x.h"

const char* loxLibSrc;

static void repl(void) {
    for (;;) {
        printf("> ");
        if (!GETS(input_line)) {
            printf("\n");
            break;
        }
        interpret(input_line);
    }
}

int main() {

#define MAGIC_VAL 0x1138
#define MAGIC_LOC ((short*)0x0200)

    if (*MAGIC_LOC == MAGIC_VAL) {
        // We are actually running on 68008 Kit, not on the emulator, so we can
        // access the LCD.
        lcd_clear();
        lcd_puts("Use terminal for");
        lcd_goto(0,1);
        lcd_puts("Lox68k REPL");
    }

    rand32 = 47110815;
    init_freelist();
    initVM();
    printf("Lox68k %s by Fred Bayer\n", VERSION);
    if (loxLibSrc) {
        printf("Loading standard library.\n");
        interpret(loxLibSrc);
    }
    repl();

    freeVM();
    return 0;
}

#else

static char* readFile(const char* path) {
    FILE* file = fopen(path, "rb");
    if (file == NULL) {
        fprintf(stderr, "Could not open \"%s\".\n", path);
        return NULL;
    }
    fseek(file, 0L, SEEK_END);
    size_t fileSize = ftell(file);
    rewind(file);

    char* buffer = (char*)malloc(fileSize + 1);
    if (buffer == NULL) {
        fprintf(stderr, "Not enough memory to read \"%s\".\n", path);
        return NULL;
    }
    size_t bytesRead = fread(buffer, sizeof(char), fileSize, file);
    if (bytesRead < fileSize) {
        fprintf(stderr, "Could not read \"%s\".\n", path);
        return NULL;
    }
    buffer[bytesRead] = '\0';
    fclose(file);
    return buffer;
}

static bool runFile(const char* path) {
    char* source = readFile(path);
    if (source) {
        InterpretResult result = interpret(source);
        free(source);
        return  result == INTERPRET_OK;
    }
    return false;
}

static void repl(void) {
    for (;;) {
        printf("> ");

        if (!GETS(input_line)) {
            printf("\n");
            break;
        }
        if (input_line[0] == '&') {
            *strchr(input_line, '\n') = '\0';
            runFile(input_line + 1);
        }
        else
            interpret(input_line);
    }
}

// To compile Lox68k for Windows, install Tiny C compiler from
// https://bellard.org/tcc/ and run
//   tcc *.c -o wlox.exe -m32
//
// To compile it for Linux, invoke
//   gcc -O2 -std=gnu89 -o llox -m32 -lm *.c
//
// Usage: [lw]lox [ <source>* [-]]
// - starts REPL after loading all sources.
//

int main(int argc, const char* argv[]) {
    int arg;

    rand32 = 47110815;
    init_freelist();
    initVM();

    printf("Lox68k (compiled for Big Computer) %s by Fred Bayer\n", VERSION);
    if (argc == 1)
        repl();
    else {
        for (arg=1; arg<argc; arg++) {
            if (argv[arg][0] == '-') 
                repl();
            else {
                printf("Loading %s\n", argv[arg]);
                if (!runFile(argv[arg]))
                    exit(10);
            }
        }
    }  

    freeVM();
    return 0;
}

#endif

