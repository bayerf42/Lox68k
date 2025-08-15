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
hexPattern = "../FBI/{}.hex"
encoding   = "ascii" # The Monitor getchar() only reads 7 bits to allow fast processing, so
                     # we restrict the entire serial protocol to pure 7 bit ASCII

CHAR_NL = b'\x0a' # ASCII new line
CHAR_BS = b'\x08' # ASCII backspace
CHAR_RS = b'\x1e' # ASCII record separator, passes thru gets(), but marks new line for Lox (scanner.c:11)

anti_stress_delay = 0.0005 # Avoid too much CPU load when waiting for a char from serial or key press
char_delay        = 0.003  # Time to wait for char sent to the Kit is echoed back.
line_delay        = 0.01   # Same for newline char.

use_history       = True   # History mode active, simple line editing is possible


def transcript_char(char):
    if char == '\r':
        pass                                 # ignore the extra CR sent from the Kit
    elif char == '\b':
        transcript.seek(transcript.tell()-1) # backspace from Kit: remove last char transcripted
    else:
        transcript.write(char)


def response():
    resp = ser.read(1)
    if resp == CHAR_RS: resp = CHAR_NL # Convert back
    char = resp.decode(encoding, errors="ignore"); 
    print(char, end="", flush=True)
    transcript_char(char)


def terminal_help():
    global use_history
    if use_history:
        print("Current mode: Line-buffered. Press F4 to switch to Direct mode.")
        print("No input is sent to the Kit until ENTER key is pressed.")
        print("Simple line editing with LEFT, RIGHT, HOME, END, BACKSPACE and DELETE keys possible.")
        print("Each input submitted is stored in history, which can be recalled with UP and DOWN keys.")
    else:
        print("Current mode: Direct. Press F4 to switch to Line-buffered mode.")
        print("Any input is sent directly, the echo displayed comes from the Kit.")
        print("Only BACKSPACE key possible to correct input.")
        print("Ctrl-ENTER: Send line break to Lox, but don't terminate input, yet.")
    print( "F2:         Upload a LOX source file to the Lox68k REPL.")
    print( "F3:         Upload a HEX file, press LOAD button on the Kit before.")
    print( "Ctrl-C:     Quit terminal")


def uploadLOX():
    print("Upload LOX file: ", end="")
    name = input()
    try:
        path = loxPattern.format(name)
        if os.stat(path).st_size >= 16384:
            print("...file too large to load.")
        else:
            with open(path, "r") as loxFile:
                for line in loxFile: 
                    for char in line.rstrip('\n'):
                        time.sleep(char_delay)
                        ser.write(char.encode(encoding, errors="replace"))
                        response()
                    time.sleep(char_delay)
                    ser.write(CHAR_RS)
                    response()
                    time.sleep(line_delay)
                ser.write(CHAR_NL)
    except FileNotFoundError:
        print("...not found.");


def uploadHEX():
    print("Upload HEX file: ", end="")
    name = input()
    try:
        with open(hexPattern.format(name), "r") as hexFile:
            for line in hexFile:
                for char in line:
                    ser.write(char.encode(encoding))
    except FileNotFoundError:
        print("...not found.");


def send_serial_line(text):
    """Send an entire line of text ignoring echo response."""
    for c in text + '\n':
        ser.write(c.encode(encoding, errors="replace"))
        time.sleep(char_delay)
        ser.read(1)  # Ignore echo from Kit
        if c != '\n':
            transcript_char(c)


def terminal_loop():
    global use_history
    history       = []
    current_input = ""
    history_index = None   # Current position in history
    staged_input  = ""     # staged input while navigating history
    prev_line_len = 0
    cursor_pos    = 0

    def refresh_line():
        """Refresh current input line on console."""
        nonlocal current_input, prev_line_len, cursor_pos
    
        # Clear previous display line
        print('\r  ' + ' ' * prev_line_len, end='\r> ')

        # Print entire input line    
        print(current_input, end='\r> ')

        # Print again upto current cursor position
        print(current_input[:cursor_pos], end='', flush=True);
    
        prev_line_len = len(current_input)
    
    while True:
        time.sleep(anti_stress_delay)
        if msvcrt.kbhit():
            key = msvcrt.getwch()

            # Function and arrow keys
            if key in ('\x00', '\xe0'):
                next_key = msvcrt.getwch()
                if   next_key == '\x3b': # F1
                    terminal_help()

                elif next_key == '\x3c': # F2
                    uploadLOX()

                elif next_key == '\x3d': # F3
                    uploadHEX()

                elif next_key == '\x3e': # F4
                    use_history = not use_history

                elif next_key == '\x48': # Arrow up
                    if use_history:
                        if history_index is None:
                            if history:
                                staged_input  = current_input
                                history_index = len(history) - 1
                                current_input = history[history_index]
                                cursor_pos    = len(current_input)
                                refresh_line()
                        elif history_index > 0:
                            history_index -= 1
                            current_input  = history[history_index]
                            cursor_pos     = len(current_input)
                            refresh_line()

                elif next_key == '\x50': # Arrow down
                    if use_history:
                        if history_index is not None:
                            if history_index < len(history) - 1:
                                history_index += 1
                                current_input  = history[history_index]
                            else:
                                history_index = None
                                current_input = staged_input
                            cursor_pos = len(current_input)
                            refresh_line()

                elif next_key == '\x4d': # Arrow right
                    if use_history:
                        # Move cursor one char to the right
                        if cursor_pos < len(current_input):
                            cursor_pos += 1
                            refresh_line()

                elif next_key == '\x4b': # Arrow left
                    if use_history:
                        # Move cursor one char to the left
                        if cursor_pos > 0:
                            cursor_pos -= 1
                            refresh_line()

                elif next_key == '\x47': # Home
                    if use_history:
                        # Move cursor to beginning of line
                        cursor_pos = 0
                        refresh_line()

                elif next_key == '\x4f': # End
                    if use_history:
                        # Move cursor to end of line
                        cursor_pos = len(current_input)
                        refresh_line()

                elif next_key == '\x53': # Delete
                    if use_history:
                        # Delete char under cursor
                        if cursor_pos < len(current_input):
                            current_input = current_input[:cursor_pos] + current_input[cursor_pos+1:]
                            refresh_line()

            # ENTER
            elif key == '\r':
                if use_history:
                    # Send input and append to history
                    send_serial_line(current_input)
                    try:
                        history.remove(current_input) # Avoid duplicate history items
                    except:
                        pass
                    history.append(current_input)

                    # Reset for next input
                    current_input = ""
                    cursor_pos    = 0
                    history_index = None
                    staged_input  = ""
                    prev_line_len = 0
                else:
                    ser.write(CHAR_NL)

            # Ctrl+ENTER
            elif key == '\x0a':
                # Send RS to Kit for next line, ignored in history mode
                if not use_history:
                    ser.write(CHAR_RS)

            # Backspace
            elif key == '\x08':
                if use_history:
                    # Delete char left of cursor and move cursor left
                    if cursor_pos > 0:
                        current_input = current_input[:cursor_pos-1] + current_input[cursor_pos:]
                        cursor_pos   -= 1
                        refresh_line()
                else:
                    ser.write(CHAR_BS)

            # Other chars
            else:
                if use_history:
                    # Insert at cursor position and move cursor right
                    current_input = current_input[:cursor_pos] + key + current_input[cursor_pos:]
                    cursor_pos   += 1 
                    refresh_line()
                else:
                    ser.write(key.encode(encoding, errors="replace"))

        # Handle Kit's response
        response()


try:
    print("Press F1 for help.")
    terminal_loop()
except KeyboardInterrupt:
    print("=== Terminal terminated, transcript closed. ===")
finally:
    transcript.close()
