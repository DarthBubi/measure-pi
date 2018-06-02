#!/usr/bin/python
# coding=utf-8

import logging
import csv
import os


class CsvLogger:

    def __init__(self, filename):
        file_exists = os.path.isfile(filename)

        self.log_file = open(filename, 'a')
        self.logger = csv.DictWriter(self.log_file, fieldnames=("time", "room_temperature"))
        if not file_exists:
            self.logger.writeheader()
        self.log_file.flush()

    def log(self, values):
        self.logger.writerow(values)
        self.log_file.flush()
