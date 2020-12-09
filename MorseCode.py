# Script for sending the IP address of a RPi to a cell phone through an
# LED on a GPIO pin. Using GPIO 26 or 21 would be good since they're 
# near the end and probably won't be used in other projects. Therefore
# IF no LED is connected while the program runs, then there's no harm.
#
# REMEMBER: For LEDs, the fat side of the LED goes to GND. The skinny 
#           side goes to the GPIO pin. No resistor is needed.
#
# Morse Code Agent worked just fine for Android and was able to hand
# various speeds and colors of LEDs.
# https://play.google.com/store/apps/details?id=com.erdatsai.morsecodeagent
# 
# How to run the script at startup (terminal commands)
# 1. TYPE sudo su
# 2. TYPE crontab -e
# 3. At the end of the file add the following
# 3a.    @reboot python /home/pi/MorseCode.py
# 
# To Disable at startup, just do the same steps and put a # in front.

import RPi.GPIO as GPIO
import subprocess, time

# Allow for time to acquire an IP Address.
time.sleep(30)

# Connect the LED to pins 39 & 40 of the 40 pin board. Change to 7 for
# original 26 pin headers.
led = 21

# Set up the GPIO pins
GPIO.setmode(GPIO.BCM),GPIO.setwarnings(False)
GPIO.setup(led, GPIO.OUT) #, GPIO.setup(red, GPIO.OUT)

# MORSE CODE RULES
# If the duration of a dot is taken to be one unit 
# then that of a dash is three units. The space between
# the components of one character is one unit, between
# characters is three units and between words seven units

# See the camera frame rate and cut it by at least half.
speed  = 15
# MORSE CODE Constants
dot    = 1/speed
dash   = 3/speed
spce   = 1/speed
cspace = 3/speed
wspace = 7/speed

# Dictionary representing the morse code chart 
MORSE_CODE_DICT = { 'A':'.-',    'B':'-...', 
                    'C':'-.-.',  'D':'-..',    'E':'.', 
                    'F':'..-.',  'G':'--.',    'H':'....', 
                    'I':'..',    'J':'.---',   'K':'-.-', 
                    'L':'.-..',  'M':'--',     'N':'-.', 
                    'O':'---',   'P':'.--.',   'Q':'--.-', 
                    'R':'.-.',   'S':'...',    'T':'-', 
                    'U':'..-',   'V':'...-',   'W':'.--', 
                    'X':'-..-',  'Y':'-.--',   'Z':'--..', 
                    '1':'.----', '2':'..---',  '3':'...--', 
                    '4':'....-', '5':'.....',  '6':'-....', 
                    '7':'--...', '8':'---..',  '9':'----.', 
                    '0':'-----', ',':'--..--', '.':'.-.-.-', 
                    '?':'..--..','/':'-..-.',  '-':'-....-', 
                    '(':'-.--.', ')':'-.--.-'} 



def dashLED():
    GPIO.output(led, GPIO.HIGH) 
    time.sleep(dash)
    GPIO.output(led, GPIO.LOW)   
    time.sleep(spce)   

def dotLED():
    GPIO.output(led, GPIO.HIGH) 
    time.sleep(dot)
    GPIO.output(led, GPIO.LOW)   
    time.sleep(spce) 

def convertToMorse(string):
    string = string.upper() # Converts string to uppercase.
    
    for i in string:
        if i == ' ':    # Look for a space
            time.sleep(wspace)
        else:
            time.sleep(cspace)
            for x in MORSE_CODE_DICT[i]:    # Decoding process
                if x == '.':
                    dotLED()
                else:
                    dashLED()

for i in range(20):
    ip_addr =  str(subprocess.check_output(["hostname", "-I"]))
    ip_addr = ip_addr[2:-4]
    msg = "IMIMIM " + ip_addr + " IMIMIM"
    convertToMorse(msg)
    time.sleep(15)
