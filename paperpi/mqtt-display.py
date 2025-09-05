#!/usr/bin/python
# -*- coding: utf-8 -*-


import argparse
import datetime
import os
import queue
import sys
import threading
from time import sleep
from typing import Any

import paho.mqtt.client as mqtt
import RPi.GPIO as GPIO
from dotenv import load_dotenv
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

def on_connect(client: mqtt.Client, userdata: Any, flags, rc):
    print("Connected with result code " + str(rc))
    # Subscribe to new topic scheme for both rooms and features
    topics = [
        ("gladys/master/device/mqtt:living_room/feature/mqtt:temperature_living_room/state", 1),
        ("gladys/master/device/mqtt:living_room/feature/mqtt:humidity_living_room/state", 1),
        ("gladys/master/device/mqtt:bedroom/feature/mqtt:temperature_bedroom/state", 1),
        ("gladys/master/device/mqtt:bedroom/feature/mqtt:humidity_bedroom/state", 1)
    ]
    for topic, qos in topics:
        client.subscribe(topic, qos)

def msg_cb(client: mqtt.Client, userdata: Any, message: mqtt.MQTTMessage):
    userdata['msg_queue'].put((message.topic, message.payload.decode("utf-8")))

def parse_args():
    parser = argparse.ArgumentParser(description='MQTT display')
    parser.add_argument('--rotation', type=int, required=False,
                        default='0',
                        help='screen rotation')
    return parser.parse_args()



if __name__ == "__main__":
    # Load environment variables from .env
    load_dotenv()

    args = parse_args()

    GPIO.setmode(GPIO.BCM)
    GPIO.setup(SW1, GPIO.IN)
    GPIO.setup(SW2, GPIO.IN)
    GPIO.setup(SW3, GPIO.IN)
    GPIO.setup(SW4, GPIO.IN)
    if SW5 != -1:
        GPIO.setup(SW5, GPIO.IN)

    papirus = Papirus(rotation=args.rotation)
    if papirus.height <= 96:
        SIZE = 18
    papirus.clear()
    write_text(papirus, "Ready... SW1 + SW2 to exit.", SIZE)

    # Thread-safe queue for messages
    msg_queue = queue.Queue()
    userdata = {'msg_queue': msg_queue}

    # Load MQTT credentials from environment
    mqtt_host = os.getenv("MQTT_HOST", "localhost")
    mqtt_port = int(os.getenv("MQTT_PORT", "1883"))
    mqtt_user = os.getenv("MQTT_USER")
    mqtt_pass = os.getenv("MQTT_PASS")

    client = mqtt.Client(client_id="paperpi", clean_session=False, userdata=userdata)
    if mqtt_user and mqtt_pass:
        client.username_pw_set(mqtt_user, mqtt_pass)
    else:
        print("MQTT_USER or MQTT_PASS not set, connecting without authentication.")
    client.on_message = msg_cb
    client.on_connect = on_connect
    client.connect(mqtt_host, mqtt_port)

    # Start MQTT loop in a separate thread
    mqtt_thread = threading.Thread(target=client.loop_forever, daemon=True)
    mqtt_thread.start()

    # State for last received values (new topic scheme)
    last_values = {
        "gladys/master/device/mqtt:living_room/feature/mqtt:temperature_living_room/state": "",
        "gladys/master/device/mqtt:living_room/feature/mqtt:humidity_living_room/state": "",
        "gladys/master/device/mqtt:bedroom/feature/mqtt:temperature_bedroom/state": "",
        "gladys/master/device/mqtt:bedroom/feature/mqtt:humidity_bedroom/state": ""
    }

    while True:
        # Exit when SW1 and SW2 are pressed simultaneously
        if (GPIO.input(SW1) == False) and (GPIO.input(SW2) == False):
            write_text(papirus, "Exiting ...", SIZE)
            sleep(0.2)
            papirus.clear()
            sys.exit()
       
        if GPIO.input(SW1) == False:
            write_text(papirus, "One", SIZE)
        elif GPIO.input(SW2) == False:
            write_text(papirus, "Two", SIZE)
        elif GPIO.input(SW3) == False:
            write_text(papirus, "Three", SIZE)
        elif GPIO.input(SW4) == False:
            # Bedroom
            text = "Bedroom\nTemp: {} °C\nHumidity: {} %".format(
                last_values["gladys/master/device/mqtt:bedroom/feature/mqtt:temperature_bedroom/state"],
                last_values["gladys/master/device/mqtt:bedroom/feature/mqtt:humidity_bedroom/state"]
            )
            write_text(papirus, text, SIZE)
        elif (SW5 != -1) and (GPIO.input(SW5) == False):
            # Living Room
            text = "Living Room\nTemp: {} °C\nHumidity: {} %".format(
                last_values["gladys/master/device/mqtt:living_room/feature/mqtt:temperature_living_room/state"],
                last_values["gladys/master/device/mqtt:living_room/feature/mqtt:humidity_living_room/state"]
            )
            write_text(papirus, text, SIZE)

        try:
            while True:
                topic, value = msg_queue.get_nowait()
                if topic in last_values:
                    last_values[topic] = value
        except queue.Empty:
            pass

        sleep(0.1)
