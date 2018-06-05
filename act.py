# coding: utf-8
"""
Agent 之间通信的 act 协议
"""

import struct

from utils import int2bytes, bytes2int


class ActRequest(object):

    def __init__(self):
        self.interface = ''
        self.method = ''
        self.parameter_types_string = ''
        self.Id = 0
        self.parameter = ''

    def encode_body(self):
        ds = []
        ds.append(int2bytes(self.Id))
        ds.append(int2bytes(len(self.parameter)))
        ds.append(self.parameter)
        return ''.join(ds)


class ActResponse(object):

    def __init__(self):
        self.Id = 0
        self.result = ''

    def encode_body(self):
        ds = []
        ds.append(int2bytes(self.Id))
        ds.append(int2bytes(len(self.result)))
        ds.append(self.result)
        return ''.join(ds)


