;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;; Some assembler utilities for Kit, replacing broken C library
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

       section code

; void fix_memcpy(char* dest, const char* src, size_t size);
       xdef      _fix_memcpy
_fix_memcpy:
       movea.l   (4,A7),A0                 ; dest
       movea.l   (8,A7),A1                 ; src
       move.l    (12,A7),D1                ; size
       bra.s     .test
.loop  move.b    (A1)+,(A0)+
.test  dbra      D1,.loop
       rts

; int fix_memcmp(const char* a, const char* b, size_t size);
       xdef      _fix_memcmp
_fix_memcmp:
       movea.l   (4,A7),A0                 ; a
       movea.l   (8,A7),A1                 ; b
       move.l    (12,A7),D1                ; size
       clr.b     D0                        ; result
       bra.s     .test
.loop  move.b    (A0)+,D0
       sub.b     (A1)+,D0
.test  dbne      D1,.loop
       ext.w     D0
       ext.l     D0
       rts 

; int putstr(const char* str);
       xdef      _putstr
_putstr:
       move.l    A2,-(A7)
       movea.l   (8,A7),A2                 ; str
.next  move.b    (A2)+,D0
       beq.s     .done
       ext.w     D0
       ext.l     D0
       move.l    D0,-(A7)
       jsr       _putch
       addq.w    #4,A7
       bra.s     .next
.done  move.l    (A7)+,A2
       rts                                 ; return value never used

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;; Some assembler replacements for bottleneck functions
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

; bool isObjType(Value value, ObjType type);
       xdef      _isObjType
_isObjType:
       move.l    (4,A7),D1                 ; value
       clr.l     D0                        ; double use as return value and bit number in next instruction
       btst      D0,D1                     ; bit 0 set?  
       bne.s     .done                     ; it's an integer
       cmp.l     #6,D1                     ; low enough to be special value?
       bls.s     .done
       move.l    D1,A0
       move.b    (4,A0),D0                 ; type tag in object
       cmp.b     (11,A7),D0                ; type argument passed
       seq       D0
.done  rts

; void push(Value value);
       xdef      _push
_push:
       lea       _vm.W,A1
       cmpi.l    #_vm+16388,(A1)           ; beyond end of value stack
       blo.s     .push
       st        20007(A1)                 ; low byte of stack overflow
       rts

.push  move.l    (A1),A0                   ; push actually
       addq.l    #4,(A1)
       move.l    (4,A7),(A0)
       rts

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;; For Simulator only: Load Motorola FFP code and Lox68k standard library from ROM file.
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

; int loadROM(void);
       xdef      _loadROM
_loadROM:
       lea       (ROM_FILE_NAME,PC),A0     ; file name
       moveq     #1,D0                     ; mode read-only
       trap      #15                       ; FOPEN
       dc.w      10 
       move.l    D0,-(A7)                  ; save file handle
       blt.s     loadError

       move.l    (A7),D1                   ; file handle
       moveq.l   #0,D0                     ; seek from begin
       movea.l   #loxlibsrc-lorom,A0       ; seek position
       trap      #15                       ; FSEEK
       dc.w      14
       blt.s     loadError

       move.l    (A7),D1                   ; file handle
       move.l    #hirom-loxlibsrc,D0       ; length
       movea.l   #loxlibsrc,A0             ; read destination
       clr.b     (A0)                      ; simulator bug, make sure read destination
                                           ; memory block is mapped by writing to it before FREAD
       trap      #15                       ; FREAD
       dc.w      12
       blt.s     loadError

loadClose:
       move.l    (A7),D1                   ; file handle
       trap      #15                       ; FCLOSE
       dc.w      16
       blt.s     loadError
       clr.l     D0                        ; Success!

loadError:
       addq.w    #4,A7
       rts
         
ROM_FILE_NAME
       dc.b      '../roms/mon_ffp_lox.bin',0

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;; For Simulator only: Load source file <name> into input buffer
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

; int loadSource(const char* name);
       xdef      _loadSource
_loadSource:
       movea.l   (4,A7),A0                 ; file name
       moveq     #1,D0                     ; mode read-only
       trap      #15                       ; FOPEN
       dc.w      10 
       move.l    D0,-(A7)                  ; save file handle
       blt.s     loadError

       move.l    (A7),D1                   ; file handle
       move.l    #16383,D0                 ; INPUT_SIZE-1
       lea       _big_buffer,A0            ; read destination
       trap      #15                       ; FREAD
       dc.w      12
       blt.s     loadError
       lea       _big_buffer,A0
       clr.b     (A0,D0.W)                 ; Terminate string with \0

       bra.s     loadClose
         
