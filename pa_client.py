# coding: utf-8

import socket
from collections import deque

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
        if key in self.conns:
            conn = self.conns[key]
        else:
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

    def _on_resolve(self, addrinfo):
        af = addrinfo[0][0]
        self.stream = IOStream(socket.socket(af), io_loop=self.io_loop)
        self.stream.set_close_callback(self._on_close)
        sockaddr = addrinfo[0][1]
        gen_log.info("sock addr {0}".format(sockaddr))
        self.stream.connect(sockaddr, self._on_connect)

    def _on_close(self):
        gen_log.info("pa conn close")

    def _on_connect(self):
        gen_log.info("start conn to pa")
        self._connected = True
        self.stream.write(encode_act_key(self.key))
        self.stream.read_bytes(4, self._on_id)

    def _on_id(self, data):
        resp = ActResponse()
        resp.Id = bytes2int(data)
        self.pepv_act_resp = resp
        self.stream.read_bytes(4, self._on_rlen)

    def _on_rlen(self, data):
        self.stream.read_bytes(bytes2int(data), self._on_res_body)

    def _on_res_body(self, data):
        resp = self.pepv_act_resp
        resp.result = data
        cb = self._callbacks[resp.Id]
        del self._callbacks[resp.Id]
        self.io_loop.add_callback(cb, resp)
        self.stream.read_bytes(4, self._on_id)

    def fetch(self, act_request, callback):
        if act_request.Id in self._callbacks:
            gen_log.error("act Id {0} already in cbs !!".format(act_request.Id))
        self._callbacks[act_request.Id] = callback
        self.queue.append(act_request)
        self._process_queue()

    def _process_queue(self):
        if not self._connected:
            gen_log.info("act connection not ready, wait an other turn")
            return
        with stack_context.NullContext():
            while self.queue:
                act_request = self.queue.popleft()
                self.stream.write(act_request.encode_body())
