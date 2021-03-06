# coding: utf-8

import socket
from collections import deque
import logging
import logging.config
import time

from tornado.iostream import IOStream
from tornado.ioloop import IOLoop
from tornado.netutil import Resolver
from tornado.log import gen_log
from tornado import stack_context

from utils import int2bytes, bytes2int
from act import ActResponse, ActRequest


class PAClient(object):

    def __init__(self, host, port, io_loop=None):
        io_loop = io_loop or IOLoop.current()
        self.host = host
        self.port = port
        self.io_loop = io_loop
        self.conns = {}

    def fetch(self, act_request, callback):
        key = (act_request.interface, act_request.method, act_request.parameter_types_string)
        # print key, type(key)
        if key in self.conns:
            conn = self.conns[key]
        else:
            # gen_log.info("key {0} not in client, create one".format(key))
            conn = PAConnection(self.host, self.port, self.io_loop, key)
            self.conns[key] = conn
        conn.fetch(act_request, callback)


def encode_act_key(key):
    ds = []
    ds.append(int2bytes(len(key[0])))
    ds.append(key[0])
    ds.append(int2bytes(len(key[1])))
    ds.append(key[1])
    ds.append(int2bytes(len(key[2])))
    ds.append(key[2])
    return ''.join(ds)


class PAConnection(object):

    def __init__(self, host, port, io_loop, key):
        self.io_loop = io_loop
        self.resolver = Resolver()
        self._callbacks = {}
        self._connected = False
        self.queue = deque()
        self.key = key
        self.stream = None
        self.pepv_act_resp = None
        self.prof = {}
        with stack_context.ExceptionStackContext(self._handle_exception):
            self.resolver.resolve(host, port, socket.AF_INET, callback=self._on_resolve)

    def _handle_exception(self, typ, value, tb):
        gen_log.exception("pa connection error [%s] [%s] %s", typ, value, tb)

    def _on_resolve(self, addrinfo):
        af = addrinfo[0][0]
        self.stream = IOStream(socket.socket(af))
        self.stream.set_nodelay(True)
        self.stream.set_close_callback(self._on_close)
        sockaddr = addrinfo[0][1]
        # gen_log.info("sock addr {0}".format(sockaddr))
        self.stream.connect(sockaddr, self._on_connect)

    def _on_close(self):
        gen_log.info("pa conn close")

    def _on_connect(self):
        # gen_log.info("start conn to pa")
        self._connected = True
        self.stream.write('\xab\xcd')               # magic number of act protocol
        # gen_log.info('write data {0}'.format(repr(encode_act_key(self.key))))
        self.stream.write(encode_act_key(self.key))
        self._process_queue()
        self.stream.read_bytes(4, self._on_id)

    def _on_id(self, data):
        resp = ActResponse()
        resp.Id = bytes2int(data)
        self.prof[resp.Id].append(time.time())
        self.pepv_act_resp = resp
        self.stream.read_bytes(4, self._on_rlen)

    def _on_rlen(self, data):
        self.stream.read_bytes(bytes2int(data), self._on_res_body)

    def _on_res_body(self, data):
        resp = self.pepv_act_resp
        resp.result = data
        cb = self._callbacks[resp.Id]
        t = time.time()
        # gen_log.info(
        #     "ActID[{0}]: {1}, {2}, {3}, {4}, {5}, {6}, {7}".format(resp.Id, self.prof[resp.Id][0], self.prof[resp.Id][1], self.prof[resp.Id][2], t, self.prof[resp.Id][1]-self.prof[resp.Id][0], self.prof[resp.Id][2]-self.prof[resp.Id][0], t-self.prof[resp.Id][0]))
        del self.prof[resp.Id]
        del self._callbacks[resp.Id]
        # self.io_loop.add_callback(cb, resp)
        cb(resp)
        self.stream.read_bytes(4, self._on_id)

    def fetch(self, act_request, callback):
        if act_request.Id in self._callbacks:
            gen_log.error("act Id {0} already in cbs !!".format(act_request.Id))
        self._callbacks[act_request.Id] = callback
        self.prof[act_request.Id] = [time.time(), ]
        self.queue.append(act_request)
        self._process_queue()

    def _process_queue(self):
        if not self._connected:
            # gen_log.info("act connection not ready, wait an other turn")
            return
        with stack_context.NullContext():
            while self.queue:
                act_request = self.queue.popleft()
                self.prof[act_request.Id].append(time.time())
                self.stream.write(act_request.encode_body())


if __name__ == "__main__":
    logging_config = dict(
        version=1,
        formatters={
            'f': {'format':
                      '%(asctime)s %(name)-12s %(levelname)-8s %(message)s'}
        },
        handlers={
            'h': {'class': 'logging.StreamHandler',
                  'formatter': 'f',
                  'level': logging.DEBUG}
        },
        loggers={
            'tornado.general': {'handlers': ['h'],
                                'level': logging.DEBUG}
        }
    )
    logging.config.dictConfig(logging_config)
    client = PAClient('localhost', 30000)
    req = ActRequest()
    req.Id = 1468690954
    req.interface = 'com.alibaba.dubbo.performance.demo.provider.IHelloService'
    req.method = 'hash'
    req.parameter_types_string = 'Ljava/lang/String;'
    req.parameter = 'VkaYzpkvaB91YKS2IPnbYYCa9KySJjazDox3gv8XR2haatbqOOzIo90a3WR1tBAjojeHSgLjL3nZYl42n8iARYBh9yojX7vealdJGHxCbHQxIuXGxagl3IdAgtiG1q3lrRMey7pKzI7Qz0di3xZaWdKC3GXlwgQ4O2OnOBLIx9RMcr8v0Dlh5EwHHXfqGm9771QhnnvI9Tt7eoUtLMEu8xl65jJEtG4rNMz9quftC5NtJBEcc7TJB4CU6vYZ5fCgbUqLke0YqOSqEFJyE6evN68m1UzoJSXuPOmhPyrQRdGR3fybqxjOzy2OthtA44guBKMOLyUAoktinn0xkfNtk7XxBkcv9VAFf2n4klKBndbbcE1AIk7e4G1zg9aH6juVcksIfTL4181ZdO5elTOYMIk42qO1Cw1Dpc1d5gyLBDKsW2k3PMZWu2FGYZNLmucY68BCDnR8afeNbh45JUdcj1meJYzviCEzwuXCwswBrIMxD3wIB7LbpNWGCOJfa4Zbwx9GOKhBNx1mK3y85NYW1tN88PPrlk50C3Ad8bo7OkwI568UlnAHJkPWtcIM6fkwaund91sGLVoga7sdrEimByNKXGm6Kct9EKH2PP6qaP1h5z2eDV3fkYBcRzbivS0aWvxLpQPTvbvuANu1e0L4CDei1l4XscDfdFmksDDiTyfkVytuZHkOkfQ1bOUTkf9YE1iBaboJiuCUlUAyacLwigTysb02EYivvYE8csAnlkcSylDuDPcCM8C1JWoLr2t7FHkM8pCrhFDokEanjpB6s2gESOLUeA0N3ZThZdOyU4m0ySGLFMtsy9zJYSCFqlIt7udaEpyZ5j9LciZEjNrVRBthhgc4xNPP9MXdRTcQ8FBcZOua6zi2sa4BtmUZxr7GOGfoEuWg6v3JCiPY'

    def callback(act_response):
        print 'get act response Id: {0} res: [{1}]'.format(act_response.Id, act_response.result)

    client.fetch(req, callback)
    IOLoop.current().start()
