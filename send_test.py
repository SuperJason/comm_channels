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
    #print('crc32_val: 0x%x'%crc32_val)

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

def packet_check(data):
    if len(data) < 4:
        return False
    channel_id = data[0]
    length = data[2] | (data[3] << 8)
    if len(data) < length + 8:
        return False
    crc32_val  = data[length+8-4]
    crc32_val |= data[length+8-3] << 8
    crc32_val |= data[length+8-2] << 16
    crc32_val |= data[length+8-1] << 24
    if crc32_val != crc32(data[:length+8-4]):
        print('crc32_val(0x%x) != crc32()(0x%x)'%(crc32_val, crc32(data[:length+8-4])))
        return False
    return True

def cli_rx_process(arg):
    print('cli_rx_proc: entry')

    fifo_name = 'cli_receive_fifo'
    while True:
        fifo_fd = open(fifo_name, 'rb')
        rx_data = fifo_fd.read()
        length = len(rx_data)
        if packet_check(rx_data):
            print('cli_rx_proc: channel[%d] packet received'%rx_data[0])
        else:
            for i in range((length+8+16)//16):
                print('cli_rx_proc: ' + str(['%02x'%d for d in rx_data[i*16:i*16+16]]))
        fifo_fd.close()

def srv_rx_process(arg):
    print('srv_rx_proc: entry')

    fifo_name = 'srv_receive_fifo'
    while True:
        fifo_fd = open(fifo_name, 'rb')
        rx_data = fifo_fd.read()
        length = len(rx_data)
        if packet_check(rx_data):
            print('srv_rx_proc: channel[%d] packet received'%rx_data[0])
        else:
            for i in range((length+8+16)//16):
                print('srv_rx_proc: ' + str(['%02x'%d for d in rx_data[i*16:i*16+16]]))
        fifo_fd.close()

if __name__ == '__main__':

    cli_rx_proc = multiprocessing.Process(target=cli_rx_process, args=('cli_rx', ))
    cli_rx_proc.start()
    srv_rx_proc = multiprocessing.Process(target=srv_rx_process, args=('srv_rx', ))
    srv_rx_proc.start()

    length = 32
    data = np.arange(length, dtype=np.uint8) + 1
    packet = packet_generate(0, data)
    for i in range((length+8+16)//16):
        print('main: ' + str(['%02x'%d for d in packet[i*16:i*16+16]]))

    sending_repeated_times = 10

    fifo_name = 'srv_send_fifo'
    for i in range(sending_repeated_times):
        print('main: packet sending via %s'%fifo_name)
        fifo_fd = open(fifo_name, 'wb')
        fifo_fd.write(packet)
        fifo_fd.close()
        time.sleep(0.5)

    fifo_name = 'cli_send_fifo'
    for i in range(sending_repeated_times):
        print('main: packet sending via %s'%fifo_name)
        fifo_fd = open(fifo_name, 'wb')
        fifo_fd.write(packet)
        fifo_fd.close()
        time.sleep(0.5)

    cli_rx_proc.terminate()
    srv_rx_proc.terminate()

    server_fifo_fd = open('srv_send_fifo', 'wb')
    server_fifo_fd.write(b'rx_quit\0')
    server_fifo_fd.close()

    server_fifo_fd = open('cli_send_fifo', 'wb')
    server_fifo_fd.write(b'rx_quit\0')
    server_fifo_fd.close()
