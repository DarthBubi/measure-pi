#!/usr/bin/python3
# coding=utf-8

import argparse
import influxdb.exceptions
import logging
import requests
import socket
import time

from datetime import datetime, timezone
from influxdb import InfluxDBClient
from typing import Optional

from read_sensor_data import read_temp


class InfluxDBLogger:
    def __init__(self, device: str, sensor_type: str, node: str, measurement: str, database: str, host: str = 'localhost', port: int = 8086, user: Optional[str] = None, password: Optional[str] = None) -> None:
        if user and password is not None:
            self.influx_client: InfluxDBClient = InfluxDBClient(
                host, port, user, password, database)
        else:
            self.influx_client: InfluxDBClient = InfluxDBClient(host, port, database=database)

        self.measurement: str = measurement
        self.device: str = device
        self.sensor_type: str = sensor_type
        self.node: str = node

    def construct_db_string(self, temperature: float):
        jason_string = [
            {
                "measurement": self.measurement,
                "tags": {
                    "device": self.device,
                    "node": self.node,
                    "sensor": self.sensor_type
                },
                "time": datetime.now(timezone.utc).strftime('%Y-%m-%dT%H:%M:%SZ'),
                "fields": {
                    "temperature": temperature
                }
            }
        ]

        return jason_string

    def log_temperature(self, sensor_id):
        temperature = read_temp(sensor_id)
        if temperature:
            data = self.construct_db_string(temperature)
            try:
                self.influx_client.write_points(data)
            except influxdb.exceptions.InfluxDBServerError as err:
                logging.error(f"InfluxDB server error: {err}")
            except requests.exceptions.ConnectionError as err:
                logging.error(f"Connection error: {err}")
            except Exception as err:
                logging.error(f"Unexpected error: {err}")
        else:
            logging.error("Error while reading temperature")


def parse_args():
    parser = argparse.ArgumentParser(description='InfluxDB logger')
    parser.add_argument('--host', type=str, required=False,
                        default='localhost',
                        help='hostname of InfluxDB http API')
    parser.add_argument('--port', type=int, required=False, default=8086,
                        help='port of InfluxDB http API')
    parser.add_argument('--device-name', type=str, required=False,
                        default=socket.gethostname(), 
                        help='the devices name, defaults to hostname')
    parser.add_argument('--sensor-type', type=str, required=True,
                        help='sensor type e.g. a DHT22 or DS18B20')
    parser.add_argument('--node', type=str, required=True,
                        help='a description of the devices purpose e.g. the location')
    parser.add_argument('--measurement', type=str, required=True,
                         help='a measurement name')
    parser.add_argument('--database', type=str, required=True,
                        help='a InfluxDB database name')
    parser.add_argument('--sensor-id', type=str, required=True,
                         help='the id of the sensor (DS18B20) or the GPIO pin (DHT22)')

    return parser.parse_args()


if __name__ == "__main__":
    args = parse_args()

    sensor_id = args.sensor_id
    sensor_name = f"/sys/bus/w1/devices/{sensor_id}/w1_slave"
    client = InfluxDBLogger(args.device_name, args.sensor_type, args.node,
                            args.measurement, args.database, host=args.host, port=args.port)

    logging.basicConfig(format='%(levelname)s %(asctime)s: %(message)s', level=logging.INFO)
    while True:
        start = time.time()
        client.log_temperature(sensor_name)
        end = time.time()
        logging.debug(end - start)
        time.sleep(30)
