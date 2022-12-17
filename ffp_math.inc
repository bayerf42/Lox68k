*-----------------------------------------------------------
* Title      : ffp_math.inc
* Written by : Fred Bayer
* Date       : 2022-12-16
* Description: Official entrypoints into Motorola Fast Floating Point Math library
*-----------------------------------------------------------


****************************************************************************************************
* Math FFP entrypoints 
****************************************************************************************************
ffp_base           equ  $5f000
 
ffp_abs            equ  (ffp_base+000)
ffp_add            equ  (ffp_base+006)
ffp_asc_round      equ  (ffp_base+012)
ffp_asc_to_flp     equ  (ffp_base+018)
ffp_atan           equ  (ffp_base+024)
ffp_compare        equ  (ffp_base+030)
ffp_cos            equ  (ffp_base+036)
ffp_cosh           equ  (ffp_base+042)
ffp_div            equ  (ffp_base+048)
ffp_exp            equ  (ffp_base+054)
ffp_flp_to_asc     equ  (ffp_base+060)
ffp_flp_to_int     equ  (ffp_base+066)
ffp_int_to_flp     equ  (ffp_base+072)
ffp_log            equ  (ffp_base+078)
ffp_mul            equ  (ffp_base+084)
ffp_neg            equ  (ffp_base+090)
ffp_pow            equ  (ffp_base+096)
ffp_sin            equ  (ffp_base+102)
ffp_sin_cos        equ  (ffp_base+108)
ffp_sinh           equ  (ffp_base+114)
ffp_sqrt           equ  (ffp_base+120)
ffp_sub            equ  (ffp_base+126)
ffp_tan            equ  (ffp_base+132)
ffp_tanh           equ  (ffp_base+138)
ffp_test           equ  (ffp_base+144)