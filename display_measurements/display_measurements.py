#!/usr/bin/python
# coding=utf-8

import sys
sys.path.append("/home/pi/projects/measure-pi-repo/measure")
from quantiser import Quantiser
from read_sensor_data import read_temp
import cv2
import numpy as np
import itertools
import os


def scan_right_to_left(img):
    """
    Scan an image from right to left. This basicly mirrors the image.

    :param img: the image to be scanned right to left
    """
    it = np.nditer(img, flags=['external_loop'], op_flags=['readwrite'])
    for p in img:
        p[...] = p[::-1]


def write_measurements_to_image(quantiser, temp=None, humid=None):
    """
    Writes the measured temperature and humidity to an image (represented as a numpy arra),
    which can be sent to an eink display.

    :param quantiser: a quantiser object to quantise the 8 bit image to a 2 bit image
    :param temp: a list of temperatures
    :param humid: a list of humidities
    """
    img = np.full((600, 800, 1), 0, np.uint8)
    font = cv2.FONT_HERSHEY_DUPLEX
    pixel_v = 75

    if temp is None:
        temp = []
    if humid is None:
        humid = []

    for elem in itertools.izip_longest(temp, humid):
        if elem[0] is not None:
            text = "Indoor: " + str(elem[0]) + " C"
            cv2.putText(img, text, (5, pixel_v), font, 2, 255, 5, cv2.CV_AA)
            cv2.circle(img, (425, pixel_v - 40), 7, 255, 5)
            pixel_v = pixel_v + 100
        else:
            text = "No indoor temperature available."
            cv2.putText(img, text, (5, pixel_v), font, 1.6, 255, 5, cv2.CV_AA)
            cv2.circle(img, (425, pixel_v - 40), 7, 255, 5)
            pixel_v = pixel_v + 100

        if elem[1] is not None:
            text = "Indoor: " + str(elem[1]) + " % humidity"
            cv2.putText(img, text, (5, pixel_v), font, 2, 255, 5, cv2.CV_AA)
            pixel_v = pixel_v + 100
        else:
            text = "No indoor humidity available."
            cv2.putText(img, text, (5, pixel_v), font, 1.6, 255, 5, cv2.CV_AA)
            pixel_v = pixel_v + 100

    img_grey = quantiser.quantise_image(img)
    scan_right_to_left(img_grey)
    # Debug image
    # cv2.imwrite("test_grey.bmp", img_grey)

    with open("img.npy", 'w') as file:
        img_grey = img_grey[:, 0::4].flatten()
        # img_grey = img_grey.byteswap() # doesn't work for some reason
        np.save(file, img_grey)


def send_image(program=None, arg=None):
    path_prog = os.path.expanduser('~') + program if os.path.exists(os.path.expanduser('~') + program) else None
    path_arg = os.path.expanduser('~') + arg if os.path.exists(os.path.expanduser('~') + arg) else None

    if path_prog and path_arg is not None:
        os.system("sudo " + path_prog + " " + path_arg)
    else:
        print("Program or image not found!")
        print(path_prog)
        print(path_arg)


if __name__ == '__main__':
    sensor_id = "28-000009aeb3f5"
    sensor_name = "/sys/bus/w1/devices/%s/w1_slave" % sensor_id
    q = Quantiser()
    temp = [read_temp(sensor_name)]
    write_measurements_to_image(q, temp)
    send_image("/projects/measure-pi-repo/display_measurements/eink", "/img.npy")
