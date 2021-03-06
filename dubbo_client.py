# coding: utf-8

from collections import deque
import socket
import logging
import logging.config
import random
import time
from functools import partial

from tornado.tcpclient import TCPClient
from tornado.netutil import Resolver, ThreadedResolver
from tornado.ioloop import IOLoop
from tornado.log import gen_log
from tornado.iostream import IOStream
from tornado import stack_context

from dubbo import Request, Response

Resolver.configure('tornado.netutil.ThreadedResolver')


class DubboClient(object):

    def __init__(self, host, port, io_loop=None):
        self.io_loop = io_loop or IOLoop.current()
        self.callbacks = {}
        self.queue = deque()
        self.conn = DubboConnection(host, port, self.io_loop)

    def fetch(self, dubbo_request, callback):
        self.conn.fetch(dubbo_request, callback)


class DubboConnection(object):

    READ_HEAD = 0x01
    READ_BODY = 0x02

    def __init__(self, host, port, io_loop):
        self.io_loop = io_loop
        self.resolver = Resolver()
        self.stream = None
        self.queue = deque()
        self._callbacks = {}
        self._connected = False
        self.read_state = self.READ_HEAD
        self.prev_response = None
        self.prof = {}
        with stack_context.ExceptionStackContext(self._handle_exception):
            self.resolver.resolve(host, port, socket.AF_INET, callback=self._on_resolve)

    def _on_resolve(self, addrinfo):
        af = addrinfo[0][0]
        self.stream = IOStream(socket.socket(af))
        self.stream.set_nodelay(True)
        self.stream.set_close_callback(self._on_close)
        sockaddr = addrinfo[0][1]
        gen_log.info("sock addr {0}".format(sockaddr))
        self.stream.connect(sockaddr, self._on_connect)

    def _on_close(self):
        gen_log.info("close dubbo connect")

    def _on_connect(self):
        gen_log.info("dubbo connect ready")
        self._connected = True
        self._process_queue()
        self.stream.read_bytes(16, self._on_header)

    def _on_header(self, data):
        # print 'read header', data
        resp = Response()
        resp.decode_header(data)
        self.prof[resp.Id].append(time.time())
        self.prev_response = resp
        self.stream.read_bytes(resp.data_len, self._on_body)

    def _on_body(self, data):
        resp = self.prev_response
        resp.decode_body(data)
        cb = self._callbacks[resp.Id]
        t = time.time()
        # gen_log.info(
        #     "DubboID[{0}]: {1}, {2}, {3}, {4}, {5}, {6}, {7}".format(resp.Id, self.prof[resp.Id][0], self.prof[resp.Id][1], self.prof[resp.Id][2], t,
        #                                                  self.prof[resp.Id][1] - self.prof[resp.Id][0], self.prof[resp.Id][2] - self.prof[resp.Id][0],
        #                                                  t - self.prof[resp.Id][0]))
        del self._callbacks[resp.Id]
        del self.prof[resp.Id]
        # self.io_loop.add_callback(cb, resp)         # 调用 callback
        cb(resp)
        self.stream.read_bytes(16, self._on_header)

    def fetch(self, dubbo_request, callback):
        if dubbo_request.Id in self._callbacks:
            gen_log.error("dubbo Id {0} already in cbs !!".format(dubbo_request.Id))
        self._callbacks[dubbo_request.Id] = callback
        self.prof[dubbo_request.Id] = [time.time(), ]
        self.queue.append(dubbo_request)
        self._process_queue()

    def _process_queue(self):
        if not self._connected:
            gen_log.info("dubbo connection not ready")
            return
        with stack_context.NullContext():
            while self.queue:
                dubbo_request = self.queue.popleft()
                self.prof[dubbo_request.Id].append(time.time())
                # print 'write data', dubbo_request.encode()
                self.stream.write(dubbo_request.encode())

    def _handle_exception(self, typ, value, tb):
        gen_log.exception("dubbo connection error [%s] [%s] %s", typ, value, tb)


def main():
    clients = []
    # client = DubboClient('127.0.0.1', 20880)
    for _ in range(1):
        clients.append(DubboClient('127.0.0.1', 20880))
    # clients.append(DubboClient('127.0.0.1', 20880))
    # clients.append(DubboClient('127.0.0.1', 20880))
    # clients.append(DubboClient('127.0.0.1', 20880))

    t0 = time.time()
    fin = []

    def callback(t, i, response):
        t1 = time.time()
        fin.append(i)
        # print 'time cost: {0}'.format(t1-t0)
        print response.result
        print '[{1}]: time cost: {0} '.format(t1-t, i), 'fin: {0}'.format(len(fin) == 512)

    for i in range(1):
        r = Request()
        r.Id = random.randint(1, 2100000000)
        r.args = 'test'
        client = random.choice(clients)
        client.fetch(r, partial(callback, time.time(), i))

    IOLoop.current().start()


if __name__ == '__main__':
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
    main()
