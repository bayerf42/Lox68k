### Python script to build a ROM image including Monitor, FFP lib, and Clox68k

import bincopy

rom_path = "../roms/"
rom_base = 0x40000

rom = bincopy.BinFile()
rom.add_binary_file(rom_path + "mon_ffp.bin", rom_base)
rom.add_file("clox_rom.hex", overwrite=True)
rom.fill()
rom.exclude(0, rom_base)

with open(rom_path + "mon_ffp_lox.bin","wb") as dest:
  dest.write(rom.as_binary())
