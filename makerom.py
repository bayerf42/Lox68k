### Python script to build a ROM image including Monitor, FFP lib, and Clox68k

import bincopy, os

rom_path = "../roms/"
rom_base = 0x40000

lib_file = "lox/stdlib.lox"
lib_base = 0x5b000

## Based on Monitor ROM with FFP library.
rom = bincopy.BinFile()
rom.add_binary_file(rom_path + "mon_ffp.bin", rom_base)

## Add Lox68k binary.
rom.add_file("clox_rom.hex", overwrite = True)

## Add Lox standard library source
lib_size = 0
try:
    rom.add_binary_file(lib_file, lib_base, overwrite = True)
    lib_size = os.stat(lib_file).st_size
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
