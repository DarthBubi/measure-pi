#!/usr/bin/python3
# coding=utf-8

import argparse
import re
import RPi.GPIO as GPIO
from logger import CsvLogger
from datetime import datetime

SENSOR_ID = f"28-000009aeb3f5"
SENSOR_NAME = f"/sys/bus/w1/devices/{SENSOR_ID}/w1_slave"


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


def parse_args():
    parser = argparse.ArgumentParser(formatter_class=argparse.ArgumentDefaultsHelpFormatter)
    parser.add_argument('--log-dir', type=str, default=None)
    args = parser.parse_args()
    dict_args = vars(args)

    return dict_args


if __name__ == '__main__':
    GPIO.setwarnings(False)
    temperature = read_temp(SENSOR_NAME)
    args = parse_args()

    if temperature is not None:
        if not args["log_dir"]:
            print("Room temperature is: " + str(temperature) + " °C")
        else:
            logger = CsvLogger(args["log_dir"] + "{}.csv".format(datetime.now().strftime("%Y-%m-%d")))
            values = dict(time=datetime.now().strftime("%Y-%m-%d-%H-%M"), room_temperature=temperature)
            logger.log(values)
