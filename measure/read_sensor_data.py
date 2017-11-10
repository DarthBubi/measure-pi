#!/usr/bin/python
# coding=utf-8

from __future__ import division
import re
import RPi.GPIO as GPIO

SENSOR_ID = "28-000009aeb3f5"
SENSOR_NAME = "/sys/bus/w1/devices/%s/w1_slave" % SENSOR_ID


def read_temp(path):
    """
    Read the current temperature from the connected DS18B20 sensor

    :param path: the path to the sensor
    :return: the measured temperature
    """
    temp = None
    with open(path, 'r') as file:
        line = file.readline()
        if re.match(r"([0-9a-f]{2} ){9}: crc=[0-9a-f]{2} YES", line):
            line = file.readline()
            match = re.match(r"([0-9a-f]{2} ){9}t=([+-]?[0-9]+)", line)
            if match:
                temp = round(float(match.group(2)) / 1000, 1)

    return temp


if __name__ == '__main__':
    GPIO.setwarnings(False)
    temperature = read_temp(SENSOR_NAME)

    if temperature is not None:
        print("Room temperature is: " + str(temperature) + " °C")
