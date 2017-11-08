#!/usr/bin/python
# coding=utf-8

import cv2
import numpy as np
from quantiser import Quantiser

# class Quantiser(object):
#     def __init__(self, num_colour=4):
#         self.num_colour = num_colour
#         self.divisor = 256 / num_colour
#         self.max_quantised_value = 255 / self.divisor

#     def quantise_pixel(self, val):
#         return ((val / self.divisor) * 255) / self.max_quantised_value

#     def quantise_image(self, image):
#         quantised_image = image.copy()
#         it = np.nditer(quantised_image,
#                        flags=['external_loop', 'buffered'],
#                        op_flags=['readwrite'],
#                        casting='safe')

#         for p in it:
#             p[...] = map(self.quantise_pixel, p)

#         return quantised_image


def write_measurements_to_image(quantiser, temp=None, humid=None):
    img = np.full((600, 800, 1), 255, np.uint8)
    font = cv2.FONT_HERSHEY_PLAIN
    cv2.putText(img, 'HelloWorld', (5, 100), font, 3, (0), 2, cv2.CV_AA)
    cv2.putText(img, 'FooBar', (5, 200), font, 3, (0), 2, cv2.CV_AA)
    # img_grey = cv2.cvtColor(img, cv2.COLOR_BGR2GRAY)

    # e1 = cv2.getTickCount()
    # for r in range(img.shape[0]):
    #     for c in range(img.shape[1]):
    #         img_grey[r, c] = quantiser.quantise_pixel(img_grey[r, c])

    # e2 = cv2.getTickCount()
    # dt = (e2 - e1)/cv2.getTickFrequency()
    # print("Ellapsed time: " + str(dt))

    # e1 = cv2.getTickCount()
    # foo = np.array([map(quantiser.quantise_pixel, p) for p in img_grey])
    # e2 = cv2.getTickCount()
    # dt = (e2 - e1)/cv2.getTickFrequency()
    # print("Ellapsed time: " + str(dt))

    e1 = cv2.getTickCount()
    img_grey = quantiser.quantise_image(img)
    e2 = cv2.getTickCount()
    dt = (e2 - e1)/cv2.getTickFrequency()
    print("Ellapsed time: " + str(dt))

    cv2.imwrite("test.bmp", img)
    cv2.imwrite("test_grey.bmp", img_grey)

    with open("img.txt", 'w') as file:
        np.savetxt(file, img_grey, fmt='%X')

if __name__ == '__main__':
    q = Quantiser()
    write_measurements_to_image(q)
