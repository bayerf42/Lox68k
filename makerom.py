### Python script to build a ROM image including Monitor, FFP lib, and Clox68k

import bincopy, os, sys

rom_path = "../roms/"
rom_base = 0x40000

lib_file = "lox/stdlib_68k.lox"
lib_base = 0x5e000 # must be same as loxlibsrc in cstart_lox_rom.asm  
lib_max  = 0x5f000 # maximum address of library, math FFP starts here  

## Based on Monitor ROM with FFP library.
rom = bincopy.BinFile()
rom.add_binary_file(rom_path + "mon_ffp.bin", rom_base)

## Add Lox68k binaries.
rom.add_file("clox.hex", overwrite = True)
rom.add_file("clox_dbg.hex", overwrite = True)

## Add Lox standard library source
lib_size = 0
try:
    lib_size = os.stat(lib_file).st_size
    if lib_base + lib_size >= lib_max:
        print(lib_file + " too large, change lib_base")
        sys.exit(10)      
    rom.add_binary_file(lib_file, lib_base, overwrite = True)
except FileNotFoundError:
    print("Library file {} not found, ignored.".format(lib_file))
finally:
    # Terminate string with NUL byte
    rom.add_binary([0], lib_base + lib_size, overwrite = True)

## Write entire ROM image
rom.fill()
rom.exclude(0, rom_base)
with open(rom_path + "mon_ffp_lox.bin","wb") as dest:
    dest.write(rom.as_binary())
