@echo off
rem Build wlox for Windows
tcc -O3 -DLOX_DBG -owloxd.exe *.c -m32
tcc -O3 -owlox.exe *.c -m32
