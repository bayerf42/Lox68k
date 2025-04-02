;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;; Glue code to 68343 math FFP library
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

       ; Glue code to 68343 math FFP library
       ; in ROM at $5f000

       include ../MotoFFP/ffp_math.inc
       section code
       xref    _errno        ; indicates overflow or domain error when != 0

_fabs
       move.l  (4,a7),d0
       and.b   #$7f,d0       ; Just clear sign bit, simpler than calling FFP lib
       rts

_sin
       move.l  d7,-(a7)
       move.l  (8,a7),d7
       jsr     ffp_sin
tail_trig                    ; Common clean-up code for sin, cos, tan, exp, log 
       bvc.s   .ok
       st      _errno.W
.ok    move.l  d7,d0
       move.l  (a7)+,d7
       rts

_cos
       move.l  d7,-(a7)
       move.l  (8,a7),d7
       jsr     ffp_cos
       bra.s   tail_trig

_tan
       move.l  d7,-(a7)
       move.l  (8,a7),d7
       jsr     ffp_tan
       bra.s   tail_trig

_exp
       move.l  d7,-(a7)
       move.l  (8,a7),d7
       jsr     ffp_exp
       bra.s   tail_trig

_log
       move.l  d7,-(a7)
       move.l  (8,a7),d7
       jsr     ffp_log
       bra.s   tail_trig


_sinh
       movem.l d3-d5/d7,-(a7)
       move.l  (20,a7),d7
       jsr     ffp_sinh
tail_hyp                     ; Common clean-up code for sinh, cosh, tanh 
       bvc.s   .ok
       st      _errno.W
.ok    move.l  d7,d0
       movem.l (a7)+,d3-d5/d7
       rts

_cosh
       movem.l d3-d5/d7,-(a7)
       move.l  (20,a7),d7
       jsr     ffp_cosh
       bra.s   tail_hyp

_tanh
       movem.l d3-d5/d7,-(a7)
       move.l  (20,a7),d7
       jsr     ffp_tanh
       bra.s   tail_hyp


_atan
       move.l  d7,-(a7)
       move.l  (8,a7),d7
       jsr     ffp_atan
       move.l  d7,d0
       move.l  (a7)+,d7
       rts

_pow
       movem.l d6-d7,-(a7)
       move.l  (12,a7),d7
       move.l  (16,a7),d6
       jsr     ffp_pow
       bvc.s   .ok
       st      _errno.W
.ok    move.l  d7,d0
       movem.l (a7)+,d6-d7
       rts

_add
       movem.l d3-d7,-(a7)
       move.l  (24,a7),d7
       move.l  (28,a7),d6
       jsr     ffp_add
tail_base                    ; Common clean-up code for add, sub, mul, div, sqrt 
       bvc.s   .ok
       st      _errno.W
.ok    move.l  d7,d0
       movem.l (a7)+,d3-d7
       rts

_sub
       movem.l d3-d7,-(a7)
       move.l  (24,a7),d7
       move.l  (28,a7),d6
       jsr     ffp_sub
       bra.s   tail_base

_mul
       movem.l d3-d7,-(a7)
       move.l  (24,a7),d7
       move.l  (28,a7),d6
       jsr     ffp_mul
       bra.s   tail_base

_div
       movem.l d3-d7,-(a7)
       move.l  (24,a7),d7
       move.l  (28,a7),d6
       jsr     ffp_div
       bra.s   tail_base

_sqrt
       movem.l d3-d7,-(a7)
       move.l  (24,a7),d7
       jsr     ffp_sqrt
       bra.s   tail_base

_less
       movem.l d6-d7,-(a7)
       move.l  (12,a7),d7
       move.l  (16,a7),d6
       clr.l   d0
       jsr     ffp_compare
       slt     d0
       movem.l (a7)+,d6-d7
       rts

_intToReal
       movem.l d5/d7,-(a7)
       move.l  (12,a7),d7
       jsr     ffp_int_to_flp
       move.l  d7,d0
       movem.l (a7)+,d5/d7
       rts

_realToInt
       movem.l d5/d7,-(a7)
       move.l  (12,a7),d7
       jsr     ffp_flp_to_int
       bvc.s   .ok
       st      _errno.W
.ok    move.l  d7,d0
       movem.l (a7)+,d5/d7
       rts

_strToReal
       movem.l d3-d7,-(a7)
       move.l  (24,a7),a0
       jsr     ffp_asc_to_flp
       bvc.s   .ok
       st      _errno.W
.ok    move.l  d7,d0
       move.l  (28,a7),d1
       beq.s   .nostore 
       move.l  d1,a1  
       move.l  a0,(a1)  
.nostore
       movem.l (a7)+,d3-d7
       rts

_realToStr
       move.l  d7,-(a7)
       move.l  (12,a7),d7
       move.l  (8,a7),a0
       jsr     ffp_flp_to_asc
       move.l  (a7)+,(a0)+     ; Transfer 14 bytes from stack
       move.l  (a7)+,(a0)+
       move.l  (a7)+,(a0)+
       move.w  (a7)+,(a0)+
       clr.b   (a0)            ; Terminate string
       move.l  (a7)+,d7
       rts
