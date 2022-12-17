;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;; Glue code to 68343 math FFP library
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

       ; Glue code to 68343 math FFP library
       ; in ROM at $5f000

       include ffp_math.inc

       section code

_fabs
       move.l  d7,-(a7)
       move.l  (8,a7),d7
       jsr     ffp_abs
       move.l  d7,d0
       move.l  (a7)+,d7
       rts

_sqrt
       clr.b   _errno
       move.l  d7,-(a7)
       move.l  (8,a7),d7
       jsr     ffp_sqrt
       bvc.s   .ok
       move.b  #1,_errno
.ok    move.l  d7,d0
       move.l  (a7)+,d7
       rts

_sin
       clr.b   _errno
       move.l  d7,-(a7)
       move.l  (8,a7),d7
       jsr     ffp_sin
       bvc.s   .ok
       move.b  #1,_errno
.ok    move.l  d7,d0
       move.l  (a7)+,d7
       rts

_cos
       clr.b   _errno
       move.l  d7,-(a7)
       move.l  (8,a7),d7
       jsr     ffp_cos
       bvc.s   .ok
       move.b  #1,_errno
.ok    move.l  d7,d0
       move.l  (a7)+,d7
       rts

_tan
       clr.b   _errno
       move.l  d7,-(a7)
       move.l  (8,a7),d7
       jsr     ffp_tan
       bvc.s   .ok
       move.b  #1,_errno
.ok    move.l  d7,d0
       move.l  (a7)+,d7
       rts

_sinh
       clr.b   _errno
       move.l  d7,-(a7)
       move.l  (8,a7),d7
       jsr     ffp_sinh
       bvc.s   .ok
       move.b  #1,_errno
.ok    move.l  d7,d0
       move.l  (a7)+,d7
       rts

_cosh
       clr.b   _errno
       move.l  d7,-(a7)
       move.l  (8,a7),d7
       jsr     ffp_cosh
       bvc.s   .ok
       move.b  #1,_errno
.ok    move.l  d7,d0
       move.l  (a7)+,d7
       rts

_tanh
       clr.b   _errno
       move.l  d7,-(a7)
       move.l  (8,a7),d7
       jsr     ffp_tanh
       bvc.s   .ok
       move.b  #1,_errno
.ok    move.l  d7,d0
       move.l  (a7)+,d7
       rts

_atan
       clr.b   _errno
       move.l  d7,-(a7)
       move.l  (8,a7),d7
       jsr     ffp_atan
       bvc.s   .ok
       move.b  #1,_errno
.ok    move.l  d7,d0
       move.l  (a7)+,d7
       rts

_exp
       clr.b   _errno
       move.l  d7,-(a7)
       move.l  (8,a7),d7
       jsr     ffp_exp
       bvc.s   .ok
       move.b  #1,_errno
.ok    move.l  d7,d0
       move.l  (a7)+,d7
       rts

_log
       clr.b   _errno
       move.l  d7,-(a7)
       move.l  (8,a7),d7
       jsr     ffp_log
       bvc.s   .ok
       move.b  #1,_errno
.ok    move.l  d7,d0
       move.l  (a7)+,d7
       rts

_pow
       clr.b   _errno
       movem.l d6-d7,-(a7)
       move.l  (12,a7),d7
       move.l  (16,a7),d6
       jsr     ffp_pow
       bvc.s   .ok
       move.b  #1,_errno
.ok    move.l  d7,d0
       movem.l (a7)+,d6-d7
       rts

_intToReal
       movem.l d5/d7,-(a7)
       move.l  (12,a7),d7
       jsr     ffp_int_to_flp
       move.l  d7,d0
       movem.l (a7)+,d5/d7
       rts

_scanReal
       movem.l d3-d7,-(a7)
       move.l  (24,a7),a0
       jsr     ffp_asc_to_flp
       move.l  d7,d0
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

