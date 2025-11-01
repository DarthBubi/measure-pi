#!/usr/bin/python3
# coding=utf-8

import argparse
import re

import adafruit_dht
import bme68xConstants as cnst
import bsecConstants as bsec
import RPi.GPIO as GPIO

from bme280 import bme280, bme280_i2c
from bme68x import BME68X
from board import Pin
from logger import CsvLogger
from datetime import datetime
from dotenv import load_dotenv

SENSOR_ID = f"28-000009aeb3f5"
SENSOR_NAME = f"/sys/bus/w1/devices/{SENSOR_ID}/w1_slave"

load_dotenv()


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


def setup_bme68x(address: int, temp_offset: float = 0.0, state_path: str = "bme68x_state.txt", conf_file: str = "bme68x_conf.txt") -> None:
    """
    Setup the BME68X sensor with BSEC configuration

    :param address: the I2C address of the BME68X sensor
    :param temp_offset: temperature offset to apply
    :param state_file: path to the BME68X state file
    :param conf_file: path to the BME68X configuration file
    """
    if address == 0x76:
        bme68x = BME68X(cnst.BME68X_I2C_ADDR_LOW, 0)
    elif address == 0x77:
        bme68x = BME68X(cnst.BME68X_I2C_ADDR_HIGH, 0)
    else:
        raise ValueError("Invalid I2C address for BME68X sensor. Use 0x76 or 0x77.")

    if state_path is None or conf_file is None:
        raise ValueError("State file and configuration file paths must be provided.")

    with open(state_path, "r") as state_file:
        states = [int(s) for s in state_file.read().strip().strip('[]').split(',') if s.strip()]

    bme68x.set_bsec_state_file(states)
    bme68x.set_temperature_offset(temp_offset)
    bme68x.set_sample_rate(bsec.BSEC_SAMPLE_RATE_LP)


def read_bme68x(address: int) -> dict[str, float | None]:
    """
    Read the current temperature, humidity and pressure from the BME68X sensor

    :param bus: the I2C bus number
    :param address: the I2C address of the BME68X sensor
    :return: dictionary with keys: temperature, pressure, humidity, iaq, static_iaq, co2_equivalent, breath_voc_equivalent
    """

    setup_bme68x(address)

    data = {}
    
    try:
        data = bme68x.get_bsec_data()
    except Exception as e:
        print(f"Error reading BME68X sensor: {e}")
        raise e
    if not data or len(data) == 0:
        raise RuntimeError("Failed to read from BME68X sensor")

    result = {
        "temperature": round(data["temperature"], 2),
        "pressure": round(data["pressure"], 2),
        "humidity": round(data["humidity"], 2),
        "iaq": round(data["iaq"], 2),
        "static_iaq": round(data["static_iaq"], 2),
        "co2_equivalent": round(data["co2_equivalent"], 2),
        "breath_voc_equivalent": round(data["breath_voc_equivalent"], 2),
        "iaq_accuracy": data["iaq_accuracy"],
        "stabilization_status": data["stabilization_status"],
        "run_in_status": data["run_in_status"]
    }
    
    return result


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
