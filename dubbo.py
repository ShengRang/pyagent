# coding: utf-8

import struct


class Request(object):

    def __init__(self):
        self.Id = 0
        self.interface_name = 'com.alibaba.dubbo.performance.demo.provider.IHelloService'
        self.method_name = 'hash'
        self.dubbo_version = '2.0.1'
        self.version = '0.0.0'
        self.parameter_types_string = 'Ljava/lang/String;'
        self.args = None
        self.two_way = True
        self.event = False

    def encode(self):
        bs = []
        bs.append(b'\xda\xbb\xc6\x00')       # magic Req 2way e sid
        bs.append(struct.pack('>Q', self.Id))
        data_length = b'\x00\x00\x00\x00'
        bs.append(data_length)
        bs.append(b'"{0}"\n'.format(self.dubbo_version))
        bs.append(b'"{0}"\n'.format(self.interface_name))
        bs.append(b'null\n')
        bs.append(b'"{0}"\n'.format(self.method_name))
        bs.append(b'"{0}"\n'.format(self.parameter_types_string))
        bs.append(b'"{0}"\n'.format(self.args))
        bs.append(b'{0}\n'.format('{"path":"com.alibaba.dubbo.performance.demo.provider.IHelloService"}'))
        tlen = sum(map(len, bs))
        bs[2] = struct.pack('>I', tlen-16)
        # print repr(''.join(bs))
        return ''.join(bs)


class Response(object):

    def __init__(self):
        self.Id = 0
        self.result = b''           # '1\n895942618\n'
        self.data_len = 0

    def decode_header(self, bts):
        if bts[0] == '\xda' and bts[1] == '\xbb':
            self.Id = struct.unpack('>Q', bts[4:12])[0]
            self.data_len = struct.unpack('>I', bts[12:16])[0]

    def decode_body(self, bts):
        self.result = bts

    def decode(self, bts):
        if bts[0] == '\xda' and bts[1] == '\xbb':
            self.Id = struct.unpack('>Q', bts[4:12])[0]
            data_len = struct.unpack('>I', bts[12:16])[0]
            self.result = bts[-data_len:]
            # print self.result


if __name__ == '__main__':
    r = Request()
    r.args = 'test'
    import socket
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.connect(('localhost', 20880))
    s.send(r.encode())
    data = s.recv(1024)
    print 'receive data:', repr(data)
    r = Response()
    r.decode(data)
    print r.Id, r.result
