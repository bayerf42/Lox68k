; CSTART_LOX_ROM.ASM  -  C startup-code for Sirichote 68008 Lox port in ROM.

; Section definitions and memory layout

lomem      equ         $400            ; Lowest usable RAM address
himem      equ         $20000          ; Highest usable RAM address
lorom      equ         $40000          ; ROM low address
hirom      equ         $60000          ; ROM high address

progstart  equ         $44000          ; Code starts here in ROM
datastart  equ         $02000          ; Data starts here in RAM
loxlibsrc  equ         $5e000          ; Lox standard library source code in ROM

stklen     equ         $4000           ; Default stacksize

magic_val  equ         $1138           ; If this value is stored
magic_addr equ         $200            ; at this address, we run on the 68008 SBC
ticks      equ         $268            ; monitor variable counting 100 Hz ticks


; Official monitor entry points in 4.x   
                                         
_get_byte              equ $40100 
_send_byte             equ $40106
_pstring               equ $4010C
_print_led             equ $40112
_disassemble           equ $40118
_lcd_init              equ $4011e
_lcd_goto              equ $40124
_lcd_puts              equ $4012a
_lcd_clear             equ $40130
_lcd_defchar           equ $40136
_monitor_loop          equ $4013c
_monitor_scan          equ $40142


           option      S0              ; write program ID to S0 record

           section     code
           org         progstart
code       equ         *               ; start address of code section (instructions in ROM)
           entry       progstart       ; for Simulator

           section     const
const      equ         *               ; start address of const section (initialized data in ROM)

           section     data
           org         datastart
data       equ         *               ; start address of data section (initialized global data ROM/RAM))

           section     bss
bss        equ         *               ; start address of bss section (uninitialized global data in RAM)

           section     heap
heap       equ         *               ; start address of heap section (start of free memory, RAM)


           section     code
start:                                 ; Start Lox interpreter with standard library
           movea.l     #loxlibsrc,A1
           bra.s       initLox

start_nolib:                           ; Start Lox interpreter without standard library
           movea.w     #0,A1

initLox    lea         himem,A7        ; initial stack pointer
           lea         bss.W,A0
           clr.l       D0              ; set bss section to zero
.loop      move.l      D0,(A0)+        
           cmp.l       #heap,A0
           bcs.s       .loop
           move.l      #-1,__ungetbuf.W ; unget-buffer for keyboard input
           move.l      #(himem-stklen),__stack.W ; top of stack (for stack overflow detection)
           move.l      A1,_loxLibSrc.W ; Lox standard library source in ROM
           jsr         _main

           xdef        __exit
__exit:                                ; exit program
           cmpi.w      #magic_val,magic_addr.W
           beq.s       .on_sbc
           trap        #15             ; IDE68K system call exit
           dc.w        0
           bra         start           ; restart

.on_sbc    trap        #0              ; call SBC Monitor command loop
           bra         start           ; restart

           xdef        __putch
__putch:                               ; Basic character output routine
           cmpi.w      #magic_val,magic_addr.W
           beq.s       .on_sbc

           move.l      4(A7),D0
           trap        #15
           dc.w        1               ; IDE68K system call 1 -> PUTCH
           rts

.on_sbc    jmp         _send_byte      ; entry in Monitor 4.x

           xdef        __getch
__getch:                               ; Basic character input routine
           cmpi.w      #magic_val,magic_addr.W
           beq.s       .on_sbc

           trap        #15
           dc.w        3               ; IDE68K system call 3 -> GETCH
           ext.w       D0
           ext.l       D0              ; D0.L is char, sign extended to 32 bits
           rts

.on_sbc    jsr         _get_byte       ; entry in Monitor 4.x
           ext.w       D0
           ext.l       D0              ; D0.L is char, sign extended to 32 bits
           rts

           xdef        stackoverflow
stackoverflow:                         ; label for compiler-generated check
__stackoverflow:                       ; label for manual check
           lea         himem,A7        ; reset stackpointer
           cmpi.w      #magic_val,magic_addr.W
           beq.s       .on_sbc

           lea         stackmsg,A0
           trap        #15             ; print message
           dc.w        7
           bra         __exit          ; abort program

.on_sbc    pea         stackmsg
           jsr         _pstring        ; entry in Monitor 4.x
           bra         __exit          ; abort program

_ticker    addq.l      #1,ticks.w      ; 10 ms interrupt handler
_rte       rte

           section     const
stackmsg:
           dc.b        'C stack overflow!',$0A,$0D
           dc.b        'Program aborted',$0A,$0D,0

           section     bss
           xdef        __ungetbuf
__ungetbuf:
           ds.l        1               ; ungetbuffer for stdio functions
           xdef        __stack
__stack:
           ds.l        1               ; bottom of stack

_errno:
           ds.b        1               ; for signalling FFP errors

           section code

