#!/usr/bin/python3
# coding=utf-8

import argparse
import influxdb.exceptions
import logging
import requests
import socket
import time

from datetime import datetime
from influxdb import InfluxDBClient
from read_sensor_data import read_temp


class InfluxDBLogger:

    def __init__(self, device, sensor_type, node, measurement, database, host='localhost', port=8086, user=None, password=None):
        if user and password is not None:
            self.influx_client = InfluxDBClient(
                host, port, user, password, database)
        else:
            self.influx_client = InfluxDBClient(host, port, database=database)

        self.measurement = measurement
        self.device = device
        self.sensor_type = sensor_type
        self.node = node

    def construct_db_string(self, temperature):
        jason_string = [
            {
                "measurement": self.measurement,
                "tags": {
                    "device": self.device,
                    "node": self.node,
                    "sensor": self.sensor_type
                },
                "time": datetime.utcnow().strftime('%Y-%m-%dT%H:%M:%SZ'),
                "fields": {
                    "temperature": temperature
                }
            }
        ]

        return jason_string

    def log_temperature(self, sensor_id):
        temperature = read_temp(sensor_id)
        if temperature is not None:
            data = self.construct_db_string(temperature)
            try:
                self.influx_client.write_points(data)
            except (influxdb.exceptions.InfluxDBServerError, requests.exceptions.ConnectionError) as err:
                logging.error(err)
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
                        default=socket.gethostname(), help='the devices name, defaults to hostname')
    parser.add_argument('--sensor-type', type=str, required=True,
                        help='sensor type e.g. a DHT22 or DS18B20')
    parser.add_argument('--node', type=str, required=True,
                        help='a description of the devices purpose e.g. the location')
    parser.add_argument('--measurement', type=str, required=True, help='')
    parser.add_argument('--database', type=str, required=True,
                        help='a InfluxDB database name')
    parser.add_argument('--sensor-id', type=str, required=True)

    return parser.parse_args()


if __name__ == "__main__":
    args = parse_args()

    sensor_id = args.sensor_id
    sensor_name = "/sys/bus/w1/devices/%s/w1_slave" % sensor_id
    client = InfluxDBLogger(args.device_name, args.sensor_type, args.node,
                            args.measurement, args.database, host=args.host, port=args.port)

    logging.basicConfig(format='%(levelname)s %(asctime)s: %(message)s', level=logging.INFO)
    while True:
        start = time.time()
        client.log_temperature(sensor_name)
        end = time.time()
        logging.debug(end - start)
        time.sleep(30)
