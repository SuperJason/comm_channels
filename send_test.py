#!/usr/bin/env python3
# -*- coding: utf-8 -*-
#

import numpy as np
import crcmod
import time

if __name__ == '__main__':

    d = np.arange(12, dtype=np.uint8)
    d[0] = 0
    d[1] = 0
    d[2] = 4
    d[3] = 0

    poly = 0x104C11DB7
    print(hex(poly) + ': ' + bin(poly))
    crc32_func = crcmod.mkCrcFun(poly, initCrc=0xffffffff, rev=True, xorOut=0x00000000)
    crc32_val = crc32_func(bytes(d[0:-4]))
    print('data\'s crc32: 0x%x'%crc32_func(bytes(d[0:8])))
    print('crc32_val: 0x%x'%crc32_val)

    d[-4] = crc32_val & 0xff
    d[-3] = (crc32_val >>  8) & 0xff
    d[-2] = (crc32_val >> 16) & 0xff
    d[-1] = (crc32_val >> 24) & 0xff
    server_fifo_fd = open('server_channel_fifo', 'wb')
    server_fifo_fd.write(d)
    server_fifo_fd.close()

    time.sleep(0.2)

    server_fifo_fd = open('server_channel_fifo', 'wb')
    server_fifo_fd.write(b'quit')
    server_fifo_fd.close()
