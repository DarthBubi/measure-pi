# coding=utf-8

import numpy as np
cimport numpy as np
cimport cython


cdef class Quantiser(object):
    cdef int num_colour
    cdef int divisor
    cdef int max_quantised_value

    def __cinit__(self, num_colour=4):
        self.num_colour = num_colour
        self.divisor = 256 / num_colour
        self.max_quantised_value = 255 / self.divisor

    def quantise_pixel(self, val):
        return ((val / self.divisor) * 255) / self.max_quantised_value

    @cython.boundscheck(False)
    def quantise_image(self, image, image_out=None):
        cdef np.ndarray[np.uint8_t] x
        cdef np.ndarray[np.uint8_t] y
        cdef int size
        cdef np.uint8_t value
        
        it = np.nditer([image, image_out], 
                        flags=['reduce_ok', 'external_loop','buffered', 'delay_bufalloc'],
                        op_flags=[['readonly'], ['readwrite', 'allocate']])
        it.reset()

        for xarr, yarr in it:
            x = xarr
            y = yarr
            size = x.shape[0]
            for i in range(size):
                value = x[i]
                y[i] = self.quantise_pixel(value)

        return it.operands[1]
