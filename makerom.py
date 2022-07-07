### Python script to bundle the Monitor hex file and the Lox68k hex file to a ROM image

## Create an EPROM/FLASH image by moving the HEX files' contents to appropriate addresses
## and patch first 8 bytes from Monitor (boot vector, initial SSP and PC)

import bincopy

# Adapt the next line to where the monitor hex file resides. You can download
# the Monitor complete with sources and documentation from
# https://www.kswichit.com/68008/Fred/MonitorV4.7.zip

MonitorPath = "rom_image/monitor4.7.hex"

rom = bincopy.BinFile(MonitorPath)
bootvector = rom[0:8]

rom.add_file("clox_rom.hex")

rom.fill()
rom.exclude(0,0x40000)

image = rom.as_binary()
image[0:8] = bootvector

with open("rom_image/mon4.7-lox1.0.bin","wb") as dest:
  dest.write(image)
