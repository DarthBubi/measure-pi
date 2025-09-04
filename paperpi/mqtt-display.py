#!/usr/bin/python
# -*- coding: utf-8 -*-

import argparse
import datetime
import os
import sys
from time import sleep
from typing import Any

import paho.mqtt.client as mqtt
import RPi.GPIO as GPIO
from paho.mqtt.properties import Properties
from papirus import Papirus
from PIL import Image, ImageDraw, ImageFont

# Check EPD_SIZE is defined
EPD_SIZE=0.0
if os.path.exists('/etc/default/epd-fuse'):
    exec(open('/etc/default/epd-fuse').read())
if EPD_SIZE == 0.0:
    print("Please select your screen size by running 'papirus-config'.")
    sys.exit()

# Running as root only needed for older Raspbians without /dev/gpiomem
if not (os.path.exists('/dev/gpiomem') and os.access('/dev/gpiomem', os.R_OK | os.W_OK)):
    user = os.getuid()
    if user != 0:
        print('Please run script as root')
        sys.exit()

# Command line usage
# papirus-buttons

hatdir = '/proc/device-tree/hat'

WHITE = 1
BLACK = 0

SIZE = 27

# Assume Papirus Zero
SW1 = 21
SW2 = 16
SW3 = 20
SW4 = 19
SW5 = 26

# Check for HAT, and if detected redefine SW1 .. SW5
if (os.path.exists(hatdir + '/product')) and (os.path.exists(hatdir + '/vendor')) :
   with open(hatdir + '/product') as f :
      prod = f.read()
   with open(hatdir + '/vendor') as f :
      vend = f.read()
   if (prod.find('PaPiRus ePaper HAT') == 0) and (vend.find('Pi Supply') == 0) :
       # Papirus HAT detected
       SW1 = 16
       SW2 = 26
       SW3 = 20
       SW4 = 21
       SW5 = -1

received_temp = received_humid = received_temp_b = received_humid_b = False
temp_l = humid_l = temp_b = humid_b = ""

def write_text(papirus: Papirus, text: str, size: int):

    # initially set all white background
    image = Image.new('1', papirus.size, WHITE)

    # prepare for drawing
    draw = ImageDraw.Draw(image)

    font = ImageFont.truetype('/usr/share/fonts/truetype/freefont/FreeMonoBold.ttf', size)

    # Calculate the max number of char to fit on line
    line_size = (papirus.width / (size*0.65))

    current_line = 0
    text_lines = [""]

    # Compute each line
    for word in text.split():
        # If there is space on line add the word to it
        if (len(text_lines[current_line]) + len(word)) < line_size:
            text_lines[current_line] += " " + word
        else:
            # No space left on line so move to next one
            text_lines.append("")
            current_line += 1
            text_lines[current_line] += " " + word

    current_line = 0
    for l in text_lines:
        current_line += 1
        draw.text( (0, ((size*current_line)-size)) , l, font=font, fill=BLACK)

    papirus.display(image)
    papirus.partial_update()

def on_connect(client: mqtt.Client, userdata: Any, flags: int, reason_code: int, properties: Properties):
    print("Connected with result code "+str(reason_code))

    # Subscribing in on_connect() means that if we lose the connection and
    # reconnect then subscriptions will be renewed.
    client.subscribe([("/home/measure/living_room/temperature", 1),
                      ("/home/measure/living_room/humidity", 1),
                      ("/home/measure/bedroom/temperature", 1),
                      ("/home/measure/bedroom/humidity", 1)])

def msg_cb(client: mqtt.Client, userdata: Any, message: mqtt.MQTTMessage):
    global received_temp, received_humid, received_temp_b, received_humid_b, temp_l, humid_l, temp_b, humid_b
    if message.topic == "/home/measure/living_room/temperature":
        temp_l = message.payload.decode("utf-8")
        received_temp = True
    elif message.topic == "/home/measure/bedroom/temperature":
        temp_b = message.payload.decode("utf-8")
        received_temp_b = True
    elif message.topic == "/home/measure/living_room/humidity":
        humid_l = message.payload.decode("utf-8")
        received_humid = True
    elif message.topic == "/home/measure/bedroom/humidity":
        humid_b = message.payload.decode("utf-8")
        received_humid_b = True

def parse_args():
    parser = argparse.ArgumentParser(description='MQTT display')
    parser.add_argument('--rotation', type=int, required=False,
                        default='0',
                        help='screen rotation')
    return parser.parse_args()

if __name__ == "__main__":
    global received, val

    args = parse_args()

    GPIO.setmode(GPIO.BCM)

    GPIO.setup(SW1, GPIO.IN)
    GPIO.setup(SW2, GPIO.IN)
    GPIO.setup(SW3, GPIO.IN)
    GPIO.setup(SW4, GPIO.IN)
    if SW5 != -1:
        GPIO.setup(SW5, GPIO.IN)

    papirus = Papirus(rotation=args.rotation)

    # Use smaller font for smaller displays
    if papirus.height <= 96:
        SIZE = 18

    papirus.clear()

    write_text(papirus, "Ready... SW1 + SW2 to exit.", SIZE)

    # Migrate to paho-mqtt v2 API: specify CallbackAPIVersion.VERSION1 for legacy callback signatures
    client = mqtt.Client(client_id="paperpi", clean_session=False, callback_api_version=mqtt.CallbackAPIVersion.VERSION1)
    client.on_message = msg_cb
    client.on_connect = on_connect
    client.connect("192.168.178.30")

    while True:
        # Exit when SW1 and SW2 are pressed simultaneously
        if (GPIO.input(SW1) == False) and (GPIO.input(SW2) == False) :
            write_text(papirus, "Exiting ...", SIZE)
            sleep(0.2)
            papirus.clear()
            sys.exit()

        if GPIO.input(SW1) == False:
            write_text(papirus, "One", SIZE)

        if GPIO.input(SW2) == False:
            write_text(papirus, "Two", SIZE)

        if GPIO.input(SW3) == False:
            write_text(papirus, "Three", SIZE)

        if GPIO.input(SW4) == False:
            write_text(papirus, "Getting bedroom values...", SIZE)
            a = datetime.datetime.now()
            while not received_humid_b:
                client.publish("/home/measure/bedroom/get", "humidity", qos=1)
                client.loop()
            humidity = humid_b
            received_humid_b = False

            while not received_temp_b:
                client.publish("/home/measure/bedroom/get", "temperature", qos=1)
                client.loop()
            b = datetime.datetime.now()
            temperature = temp_b
            received_temp_b = False
            print(b - a)

            text = "bedroom"
            text += "\ntemperature: " + temperature + " °C"
            text += "\nhumidity: " + humidity + " %"
            write_text(papirus, text, SIZE)

        if (SW5 != -1) and (GPIO.input(SW5) == False):
            write_text(papirus, "Getting living room values...", SIZE)
            a = datetime.datetime.now()
            while not received_humid:
                client.publish("/home/measure/living_room/get", "humidity", qos=1)
                client.loop()
            humidity = humid_l
            received_humid = False

            while not received_temp:
                client.publish("/home/measure/living_room/get", "temperature", qos=1)            
                client.loop()
            b = datetime.datetime.now()
            temperature = temp_l
            received_temp = False
            print(b - a)

            text = "living room"
            text += "\ntemperature: " + temperature + " °C"
            text += "\nhumidity: " + humidity + " %"
            write_text(papirus, text, SIZE)

        sleep(0.1)