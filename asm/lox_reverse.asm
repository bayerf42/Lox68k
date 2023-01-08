*-----------------------------------------------------------
* Title      : lox_reverse.asm
* Written by : Fred Bayer
* Date       : 2022-09-20
* Description: Assembly subroutine to reverse a Lox list.
*              call it by invoking exec($500, list) from Lox.
*-----------------------------------------------------------

            org     $500

reverse     move.l  4(sp),d0         ; First parameter
            andi.l  #$80000001,d0    ; Test for Object
            bne.s   .wrong_type
            movea.l 4(sp),a1         ; Load first parameter (address of Lox list object)
            cmpi.b  #6,4(a1)         ; test for list subtype
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

            move.l  4(sp),d0         ; object itself for success
            rts

.wrong_type move.l  #$80000002,d0    ; nil for error
            rts