# A simple terminal program.
#
# Can be used for any communication with the Sirichote 68008 kit,
# but especially suited for the Lox68k interpreter, since uploading
# Lox source files is possible via the F2 key.
#
# Line feeds in the source files are converted to ASCII record separator
# chars, which 'gets()' on the 68008 Kit accepts and indicate a new line
# there, but don't terminate input yet.
#

import msvcrt
import serial
import time
import os

# Adapt pathes etc. for your needs here
ser = serial.Serial("COM8:", baudrate=9600, timeout=0)
transcript = open("transcript.log", "w")
loxPattern = "lox/{}.lox"
hexPattern = "../../FBI/{}.hex"
encoding   = "ascii" # The Monitor getchar() only reads 7 bits to allow processing fast enough, so
                     # we restrict the entire serial protocol to ASCII

RS = b'\x1e' # ASCII record separator, passes thru gets(), but marks new line for Lox (scanner.c:90)
anti_stress_delay = 0.0005 # Avoid too much CPU load when waiting for a char from serial or key press
char_delay = 0.003 # Time to wait for char sent to the Kit is echoed back.
line_delay = 0.01  # Same for newline char.


def response():
    resp = ser.read(1)
    if resp == RS: resp = b'\n' # Convert back
    char = resp.decode(encoding, errors="ignore"); 
    print(char, end="", flush=True)
    if char != '\r': transcript.write(char)


def terminal_help():
    print("Any input is sent to 68008 kit via serial port, any response from it is displayed here.")
    print("Ctrl-ENTER:  Send a line break to Lox, but don't terminate input, yet.")
    print("F2:          Upload a LOX source file to the Lox68k REPL.")
    print("F3:          Upload a HEX file, press LOAD button on the Kit before.")
    print("Ctrl-C:      Quit terminal")


def uploadLOX():
    print("Upload LOX file: ", end="")
    name = input()
    try:
        path = loxPattern.format(name)
        if os.stat(path).st_size >= 16384:
            print("...file too large to load.")
        else:
            for line in open(path, "r"):
                for char in line.rstrip('\n'):
                    time.sleep(char_delay)
                    ser.write(char.encode(encoding, errors="replace"))
                    response()
                time.sleep(char_delay)
                ser.write(RS)
                response()
                time.sleep(line_delay)
            ser.write(b'\n')
    except FileNotFoundError:
        print("...not found.");


def uploadHEX():
    print("Upload HEX file: ", end="")
    name = input()
    try:
        for line in open(hexPattern.format(name), "r"):
            for char in line:
                ser.write(char.encode(encoding))
    except FileNotFoundError:
        print("...not found.");


def terminal_loop():
    while True:
        time.sleep(anti_stress_delay)
        if msvcrt.kbhit():
            key = msvcrt.getwch()

            if key == '\x00' or key == '\xe0':
                key = msvcrt.getwch()
                if key == ';':   # F1 
                    terminal_help()
                elif key == '<': # F2 
                    uploadLOX()
                elif key == '=': # F3 
                    uploadHEX()

            elif key == '\x0a': # Ctrl-ENTER -> new line in input, but not completed yet
                ser.write(RS) 

            else:
                ser.write(key.encode(encoding, errors="replace"))

        response() 


try:
    terminal_loop()
except KeyboardInterrupt:
    print("=== Terminal terminated, transcript closed. ===")
finally:
    transcript.close()
