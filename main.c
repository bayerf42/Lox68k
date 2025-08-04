#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "nano_malloc.h"
#include "native.h"
#include "memory.h"
#include "vm.h"

#define VERSION "Lox68k 1.7"
#define AUTHOR  "by Fred Bayer"
#ifdef LOX_DBG
#define DBG_STR "debug"
#else
#define DBG_STR "nodeb"
#endif

#ifdef KIT68K

#include "monitor4x.h"

const char* loxLibSrc;

int main() {
    if (ON_KIT()) { // Running on actual 68008 Kit
        // Welcome message on LCD
        lcd_clear();    lcd_puts(VERSION);
        lcd_goto(11,0); lcd_puts(DBG_STR);
        lcd_goto(0,1);  lcd_puts(AUTHOR);

        // init 100 Hz ticker
        clock()     = 0;
        IRQ2_VECTOR = (void *)ticker;
        // ANDI  #$f0ff,SR  ; clear interrupt mask in status register
        _word(0x027c); _word(0xf0ff);

    } else { // Running on IDE68k Simulator
        TRAP1_VECTOR = (void *)rte; // short-cut TRAP #1
        *port0       = 0x40;        // make IRQ button check fail

        // Load ROM file with FFP lib and Lox standard lib.
        putstr(loadROM() == 0 ? "ROM loaded.\n" : "ROM not found.\n");
    }

    init_freelist();
    initVM();
    printf("%s [%s] %s\n", VERSION, DBG_STR, AUTHOR);
    if (loxLibSrc) {
        putstr("Loading standard library.\n");
        interpret(loxLibSrc);
    }

    // REPL
    for (;;) {
        putstr("> ");
        readLine();
        if (!ON_KIT() && big_buffer[0] == '&') {
            if (loadSource(big_buffer + 1) == 0) 
                putstr("File loaded.\n");
            else {
                putstr("File not found.\n");
                continue;
            }
        }    
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
        EvalResult result = interpret(source);
        free(source);
        return result == EVAL_OK;
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

// Usage: [lw]loxd? [ <source>* [-]]
// - starts REPL after loading all sources.
//

int main(int argc, const char* argv[]) {
    int arg;

    init_freelist();
    initVM();

    printf("%s [%s] %s\n", VERSION, DBG_STR, AUTHOR);
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
