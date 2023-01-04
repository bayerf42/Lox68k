### Python script to bundle the Monitor hex file and the Lox68k hex file to a ROM image

## Create an EPROM/FLASH image by moving the HEX files' contents to appropriate addresses
## and patch first 8 bytes from Monitor (boot vector, initial SSP and PC)

import bincopy
import shutil

# Adapt the next line to where the monitor hex file resides. You can download
# the Monitor complete with sources and documentation from
# https://kswichit.net/68008/Fred/MonitorV4.7.zip

rom_path = "../roms/"
mon_file = "monitor.hex"
ffp_file = "motoffp.hex"
lox_file = "clox_rom.hex"
loxram_file = "clox.hex"

rom = bincopy.BinFile(rom_path + mon_file)
bootvector = rom[0:8]

rom.add_file(lox_file)
rom.add_file(rom_path + ffp_file)

rom.fill()
rom.exclude(0,0x40000)

image = rom.as_binary()
image[0:8] = bootvector

with open(rom_path + "mon_ffp_lox.bin","wb") as dest:
  dest.write(image)

# Also copy hex files to rom dir
shutil.copy2(lox_file, rom_path)
shutil.copy2(loxram_file, rom_path)