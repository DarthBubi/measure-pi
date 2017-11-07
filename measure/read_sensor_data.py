#!/usr/bin/python
# coding=utf-8

from __future__ import division
from time import sleep
import re, os
import RPi.GPIO as GPIO

sensor_id = "28-000009aeb3f5"
sensor_name = "/sys/bus/w1/devices/%s/w1_slave" % sensor_id

def read_temp(path):
    temp = None
    with open(path, 'r') as file:
        line = file.readline()
        if re.match(r"([0-9a-f]{2} ){9}: crc=[0-9a-f]{2} YES", line):
            line = file.readline()
            match = re.match(r"([0-9a-f]{2} ){9}t=([+-]?[0-9]+)", line)
            if match:
                temp = round(float(match.group(2))/1000, 1)

    return temp


if __name__ == '__main__':
    GPIO.setwarnings(False)
    # GPIO.setmode(GPIO.BOARD)
    # GPIO.setup(pin_led, GPIO.OUT)

    # while True:
    temp = read_temp(sensor_name)

    if temp is not None:
        print("Room temperature is: " + str(temp) + " °C")
        # sleep(1)

