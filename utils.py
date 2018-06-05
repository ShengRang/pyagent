# coding: utf-8

import struct


def bytes2int(b):
    return struct.unpack('>I', b)


def int2bytes(i):
    return struct.pack('>I', i)
