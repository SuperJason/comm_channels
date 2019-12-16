#!/usr/bin/env python3
# -*- coding: utf-8 -*-
#

import numpy as np
import crcmod
import time
import multiprocessing

def crc32(data):
    poly = 0x104C11DB7
    crc32_func = crcmod.mkCrcFun(poly, initCrc=0xffffffff, rev=True, xorOut=0x00000000)
    crc32_val = crc32_func(bytes(data))
    print('crc32_val: 0x%x'%crc32_val)

    return crc32_val

def packet_generate(channel_id, data):
    length = len(data)
    packet = np.zeros(length + 8, dtype=np.uint8)
    packet[0] = channel_id
    # packet[1] = 0
    packet[2] = length & 0xff
    packet[3] = (length >> 8) & 0xff
    packet[4:-4] = data
    crc32_val = crc32(packet[0:-4])
    packet[-4] = crc32_val & 0xff
    packet[-3] = (crc32_val >>  8) & 0xff
    packet[-2] = (crc32_val >> 16) & 0xff
    packet[-1] = (crc32_val >> 24) & 0xff

    return packet

def rx_process(arg):
    print('rx_process: ' + arg)

    client_fifo_name = 'client_channel_fifo'
    client_fifo_fd = open(client_fifo_name, 'rb')
    rx_data = client_fifo_fd.read()
    length = len(rx_data)
    for i in range((length+8+16)//16):
        print('rx_process: ' + str(['%02x'%d for d in rx_data[i*16:i*16+16]]))
    client_fifo_fd.close()

if __name__ == '__main__':

    rx_proc = multiprocessing.Process(target=rx_process, args=('rx', ))
    rx_proc.start()

    length = 32
    data = np.arange(length, dtype=np.uint8) + 1
    packet = packet_generate(0, data)
    for i in range((length+8+16)//16):
        print('main: ' + str(['%02x'%d for d in packet[i*16:i*16+16]]))

    server_fifo_name = 'server_channel_fifo'

    server_fifo_fd = open(server_fifo_name, 'wb')
    server_fifo_fd.write(packet)
    server_fifo_fd.close()

    time.sleep(0.2)

    server_fifo_fd = open('server_channel_fifo', 'wb')
    server_fifo_fd.write(b'quit')
    server_fifo_fd.close()
    rx_proc.join()
