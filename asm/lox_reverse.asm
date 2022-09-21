*-----------------------------------------------------------
* Title      : lox_reverse.asm
* Written by : Fred Bayer
* Date       : 2022-09-20
* Description: Assembly subroutine to reverse a Lox list.
*              call it by invoking exec($500, addr(list)) from Lox.
*-----------------------------------------------------------

            org     $500

reverse     movea.l 4(sp),a1         ; First parameter (address of Lox list object)
            cmpi.b  #5,4(a1)         ; test for list subtype
            bne.s   .wrong_type
            move.w  6(a1),d0         ; length of list
            asl.w   #2,d0            ; * 4 ( = sizeof(Value) )
            movea.l 10(a1),a0        ; first item
            lea     (a0,d0.w),a1     ; beyond last item
            asr.w   #3,d0            ; half len
            bra.s   .loop

.again      move.l  -(a1),d1         ; swap lo and hi items
            move.l  (a0),(a1)
            move.l  d1,(a0)+
.loop       dbf     d0,.again

            clr.l   d0               ; success!
            rts

.wrong_type moveq   #$ff,d0          ; -1 for error
            rts