# coding: utf-8

import struct
import socket


def bytes2int(b):
    return struct.unpack('>I', b)[0]


def int2bytes(i):
    return struct.pack('>I', i)


def get_ip():
    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    try:
        # doesn't even have to be reachable
        s.connect(('10.255.255.255', 1))
        ip = s.getsockname()[0]
    except:
        ip = '127.0.0.1'
    finally:
        s.close()
    return ip
