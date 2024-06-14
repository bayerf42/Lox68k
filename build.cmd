@echo off
rem Build wlox for Windows
tcc -O3 -owlox.exe *.c -m32
