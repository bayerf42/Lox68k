;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;; Some assembler utilities for Kit, replacing broken C library
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;


       section code

; void fix_memcpy(char* dest, const char* src, size_t size);
       xdef      _fix_memcpy
_fix_memcpy:
       movea.l   (4,A7),A0
       movea.l   (8,A7),A1
       move.l    (12,A7),D1
       bra.s     .test
.loop  move.b    (A1)+,(A0)+
.test  dbra      D1,.loop
       rts


; int fix_memcmp(const char* a, const char* b, size_t size);
       xdef      _fix_memcmp
_fix_memcmp:
       movea.l   (4,A7),A0
       movea.l   (8,A7),A1
       move.l    (12,A7),D1
       clr.b     D0
       bra.s     .test
.loop  move.b    (A0)+,D0
       sub.b     (A1)+,D0
.test  dbne      D1,.loop
       ext.w     D0
       ext.l     D0
.done  rts 


; int putstr(const char* str);
       xdef      _putstr
_putstr:
       move.l    A2,-(A7)
       movea.l   (8,A7),A2
.next  move.b    (A2)+,D0
       beq.s     .done
       ext.w     D0
       ext.l     D0
       move.l    D0,-(A7)
       jsr       _putch
       addq.w    #4,A7
       bra.s     .next
.done  move.l    (A7)+,A2
       clr.l     D0 
       rts
