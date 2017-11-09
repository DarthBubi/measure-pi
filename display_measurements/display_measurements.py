#!/usr/bin/python
# coding=utf-8

import sys
sys.path.append("../measure")
from quantiser import Quantiser
from read_sensor_data import read_temp
import cv2
import numpy as np

def scan_right_to_left(img):
    it = np.nditer(img, flags=['external_loop'], op_flags=['readwrite'])
    for p in img:
        p[...] = p[::-1]


def write_measurements_to_image(quantiser, temp=None, humid=None):
    img = np.full((600, 800, 1), 0, np.uint8)
    font = cv2.FONT_HERSHEY_DUPLEX
    pixel_v = 50

    if temp is not None:
        for t in temp:
            text = "Room temperature: " + str(t) + " Celsius"
            cv2.putText(img, text, (5, pixel_v), font, 1.5, 255, 4, cv2.CV_AA)
            pixel_v = pixel_v + 100

    img_grey = quantiser.quantise_image(img)
    scan_right_to_left(img_grey)
    # Debug image
    # cv2.imwrite("test_grey.bmp", img_grey)

    with open("img.npy", 'w') as file:
        img_grey = img_grey[:, 0::4].flatten()
        # img_grey = img_grey.byteswap() # doesn't work for some reason
        np.save(file, img_grey)


if __name__ == '__main__':
    sensor_id = "28-000009aeb3f5"
    sensor_name = "/sys/bus/w1/devices/%s/w1_slave" % sensor_id
    q = Quantiser()
    temp = [read_temp(sensor_name)]
    write_measurements_to_image(q, temp)
