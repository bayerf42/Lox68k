#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "nano_malloc.h"
#include "native.h"
#include "memory.h"
#include "vm.h"

#define VERSION "V1.5"

#ifdef KIT68K

#include "monitor4x.h"

const char* loxLibSrc;

int main() {
    if (ON_KIT()) { // Running nn actual 68008 Kit
        // Welcome message on LCD
        lcd_clear();
        lcd_puts("Use terminal for");
        lcd_goto(0,1);
        lcd_puts("Lox68k REPL");

        // init 100 Hz ticker
        clock()     = 0;
        IRQ2_VECTOR = (void *)ticker;
        // ANDI  #$f0ff,SR  ; clear interrupt mask in status register
        _word(0x027c); _word(0xf0ff);

    } else { // Running on IDE68k Simulator
        TRAP1_VECTOR = (void *)rte; // short-cut TRAP #1
        *port0       = 0x40;        // make IRQ button check fail

        // Load ROM file with FFP lib and Lox standard lib.
        if (loadROM() == 0)
            putstr("ROM loaded.\n");
        else {
            putstr("ROM not found.\n");
            loxLibSrc = NULL;
        }  
    }

    init_freelist();
    initVM();
    printf("Lox68k %s by Fred Bayer\n", VERSION);
    if (loxLibSrc) {
        putstr("Loading standard library.\n");
        interpret(loxLibSrc);
    }

    // REPL
    for (;;) {
        putstr("> ");
        readLine();
        interpret(big_buffer);
    }
    return 0; // not reached
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
        putstr("> ");
        if (!readLine()) {
            putstr("\n");
            break;
        }

        if (big_buffer[0] == '&')
            runFile(big_buffer + 1);
        else
            interpret(big_buffer);
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

    init_freelist();
    initVM();

    printf("Lox68k %s by Fred Bayer\n", VERSION);
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

