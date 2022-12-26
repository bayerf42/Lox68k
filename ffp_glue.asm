;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;; Glue code to 68343 math FFP library
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

       ; Glue code to 68343 math FFP library
       ; in ROM at $5f000

       include ../MotoFFP/ffp_math.inc

       section code

       xref    _errno        ; indicates overflow or domain error when != 0
       xref    _terminator   ; pointer to char terminating real number scanning 


_fabs
       move.l  d7,-(a7)
       move.l  (8,a7),d7
       jsr     ffp_abs
       move.l  d7,d0
       move.l  (a7)+,d7
       rts

_sqrt
       move.l  d7,-(a7)
       move.l  (8,a7),d7
       jsr     ffp_sqrt
       bvc.s   .ok
       move.b  #1,_errno
.ok    move.l  d7,d0
       move.l  (a7)+,d7
       rts

_sin
       move.l  d7,-(a7)
       move.l  (8,a7),d7
       jsr     ffp_sin
       bvc.s   .ok
       move.b  #1,_errno
.ok    move.l  d7,d0
       move.l  (a7)+,d7
       rts

_cos
       move.l  d7,-(a7)
       move.l  (8,a7),d7
       jsr     ffp_cos
       bvc.s   .ok
       move.b  #1,_errno
.ok    move.l  d7,d0
       move.l  (a7)+,d7
       rts

_tan
       move.l  d7,-(a7)
       move.l  (8,a7),d7
       jsr     ffp_tan
       bvc.s   .ok
       move.b  #1,_errno
.ok    move.l  d7,d0
       move.l  (a7)+,d7
       rts

_sinh
       move.l  d7,-(a7)
       move.l  (8,a7),d7
       jsr     ffp_sinh
       bvc.s   .ok
       move.b  #1,_errno
.ok    move.l  d7,d0
       move.l  (a7)+,d7
       rts

_cosh
       move.l  d7,-(a7)
       move.l  (8,a7),d7
       jsr     ffp_cosh
       bvc.s   .ok
       move.b  #1,_errno
.ok    move.l  d7,d0
       move.l  (a7)+,d7
       rts

_tanh
       move.l  d7,-(a7)
       move.l  (8,a7),d7
       jsr     ffp_tanh
       bvc.s   .ok
       move.b  #1,_errno
.ok    move.l  d7,d0
       move.l  (a7)+,d7
       rts

_atan
       move.l  d7,-(a7)
       move.l  (8,a7),d7
       jsr     ffp_atan
       move.l  d7,d0
       move.l  (a7)+,d7
       rts

_exp
       move.l  d7,-(a7)
       move.l  (8,a7),d7
       jsr     ffp_exp
       bvc.s   .ok
       move.b  #1,_errno
.ok    move.l  d7,d0
       move.l  (a7)+,d7
       rts

_log
       move.l  d7,-(a7)
       move.l  (8,a7),d7
       jsr     ffp_log
       bvc.s   .ok
       move.b  #1,_errno
.ok    move.l  d7,d0
       move.l  (a7)+,d7
       rts

_pow
       movem.l d6-d7,-(a7)
       move.l  (12,a7),d7
       move.l  (16,a7),d6
       jsr     ffp_pow
       bvc.s   .ok
       move.b  #1,_errno
.ok    move.l  d7,d0
       movem.l (a7)+,d6-d7
       rts

_neg
       move.l  d7,-(a7)
       move.l  (8,a7),d7
       jsr     ffp_neg
       move.l  d7,d0
       move.l  (a7)+,d7
       rts

_add
       movem.l d3-d7,-(a7)
       move.l  (24,a7),d7
       move.l  (28,a7),d6
       jsr     ffp_add
       bvc.s   .ok
       move.b  #1,_errno
.ok    move.l  d7,d0
       movem.l (a7)+,d3-d7
       rts

_sub
       movem.l d3-d7,-(a7)
       move.l  (24,a7),d7
       move.l  (28,a7),d6
       jsr     ffp_sub
       bvc.s   .ok
       move.b  #1,_errno
.ok    move.l  d7,d0
       movem.l (a7)+,d3-d7
       rts

_mul
       movem.l d3-d7,-(a7)
       move.l  (24,a7),d7
       move.l  (28,a7),d6
       jsr     ffp_mul
       bvc.s   .ok
       move.b  #1,_errno
.ok    move.l  d7,d0
       movem.l (a7)+,d3-d7
       rts

_div
       movem.l d3-d7,-(a7)
       move.l  (24,a7),d7
       move.l  (28,a7),d6
       jsr     ffp_div
       bvc.s   .ok
       move.b  #1,_errno
.ok    move.l  d7,d0
       movem.l (a7)+,d3-d7
       rts

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
       move.b  #1,_errno
.ok    move.l  d7,d0
       movem.l (a7)+,d5/d7
       rts

_scanReal
       movem.l d3-d7,-(a7)
       move.l  (24,a7),a0
       jsr     ffp_asc_to_flp
       bvc.s   .ok
       move.b  #1,_errno
.ok    move.l  d7,d0
       move.l  a0,_terminator
       movem.l (a7)+,d3-d7
       rts

_printReal
       move.l  d7,-(a7)
       move.l  (12,a7),d7
       move.l  (8,a7),a0
       jsr     ffp_flp_to_asc
       move.l  (a7)+,(a0)+     ; Transfer 14 bytes from stack
       move.l  (a7)+,(a0)+
       move.l  (a7)+,(a0)+
       move.w  (a7)+,(a0)+
       move.b  #0,(a0)         ; Terminate string
       move.l  (a7)+,d7
       rts

