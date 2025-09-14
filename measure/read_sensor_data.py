#!/usr/bin/python3
# coding=utf-8

import argparse
import re
import RPi.GPIO as GPIO
import adafruit_dht

from bme280 import bme280, bme280_i2c
from board import Pin
from logger import CsvLogger
from datetime import datetime

SENSOR_ID = f"28-000009aeb3f5"
SENSOR_NAME = f"/sys/bus/w1/devices/{SENSOR_ID}/w1_slave"


def read_ds18b20(path: str) -> float | None:
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


def read_bme280(bus: int, address: int) -> tuple[float | None, float | None, float | None]:
    """
    Read the current temperature and humidity from the BME280 sensor

    :param bus: the I2C bus number
    :param address: the I2C address of the BME280 sensor
    :return: a tuple of (temperature, pressure, humidity)
    """
    bme280_i2c.set_default_i2c_address(bus)
    bme280_i2c.set_default_bus(address)
    
    bme280.setup()
    
    sensor_values: bme280.Data = bme280.read_all()

    temperature = round(sensor_values.temperature, 2)
    pressure = round(sensor_values.pressure, 2)
    humidity = round(sensor_values.humidity, 2)

    return temperature, pressure, humidity


def read_dht22(pin: Pin) -> tuple[float, float]:
    """
    Read the current temperature and humidity from the DHT22 sensor

    :param pin: the GPIO pin number
    :return: a tuple of (temperature, humidity)
    """
    dht_device = adafruit_dht.DHT22(pin, False)

    try:
        humidity, temperature = dht_device.humidity, dht_device.temperature
    except RuntimeError as e:
        print(f"Error reading DHT22 sensor: {e}")
        raise e

    if humidity is None or temperature is None:
        raise RuntimeError("Failed to read from DHT22 sensor")
    
    humidity = round(humidity, 2)
    temperature = round(temperature, 2)

    return temperature, humidity

def parse_args() -> dict:
    parser = argparse.ArgumentParser(formatter_class=argparse.ArgumentDefaultsHelpFormatter)
    parser.add_argument('--log-dir', type=str, default=None)
    args = parser.parse_args()
    dict_args = vars(args)

    return dict_args


if __name__ == '__main__':
    GPIO.setwarnings(False)
    temperature = read_ds18b20(SENSOR_NAME)
    args = parse_args()

    if temperature is not None:
        if not args["log_dir"]:
            print("Room temperature is: " + str(temperature) + " °C")
        else:
            logger = CsvLogger(args["log_dir"] + "{}.csv".format(datetime.now().strftime("%Y-%m-%d")))
            values = dict(time=datetime.now().strftime("%Y-%m-%d-%H-%M"), room_temperature=temperature)
            logger.log(values)
