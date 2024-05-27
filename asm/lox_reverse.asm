*-----------------------------------------------------------
* Title      : lox_reverse.asm
* Written by : Fred Bayer
* Date       : 2022-09-20
* Description: Assembly subroutine to reverse a Lox list in place.
*              call it by invoking exec($500, list) from Lox.
*-----------------------------------------------------------

            org     $500

reverse     move.l  4(sp),d1         ; First parameter
            clr.l   d0               ; dual use as bit index and return value in case of type error 
            btst    d0,d1
            bne.s   .wrong_type      ; it's an integer
            cmp.l   #6,d1
            bls.s   .wrong_type      ; it's a special value (nil, false, true)

            movea.l 4(sp),a1         ; Load first parameter (address of Lox list object)
            cmpi.b  #6,4(a1)         ; test for list subtype
            bne.s   .wrong_type

            move.w  6(a1),d0         ; length of list
            asl.w   #2,d0            ; * 4 ( = sizeof(Value) )
            movea.l 10(a1),a0        ; first item
            lea     (a0,d0.w),a1     ; beyond last item
            asr.w   #3,d0            ; half length as loop counter, middle item stays fixed (odd length)
            bra.s   .loop

.again      move.l  -(a1),d1         ; swap lo and hi items
            move.l  (a0),(a1)
            move.l  d1,(a0)+
.loop       dbf     d0,.again

            move.l  4(sp),d0         ; object itself for success
.wrong_type rts