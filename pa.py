#!/usr/bin/env python
# encoding: utf-8

import logging
import logging.config

import tornado
import tornado.web
import tornado.httpserver
import tornado.ioloop
import tornado.options
from tornado.options import define, options
from tornado import gen
from tornado.tcpserver import TCPServer
from tornado.iostream import StreamClosedError
from tornado.log import gen_log
import etcd

from utils import bytes2int, get_ip
from act import ActRequest, ActResponse
from dubbo_client import DubboClient
from dubbo import Request as DubboRequest


def java_string_hashcode(s):
    h = 0
    for c in s:
        h = (31 * h + ord(c)) & 0xFFFFFFFF
    return ((h + 0x80000000) & 0xFFFFFFFF) - 0x80000000


class PAServer(TCPServer):

    def __init__(self, port=30000, weight=1, etcd_host="localhost"):
        super(PAServer, self).__init__()
        client = etcd.Client(host=etcd_host, port=2379)
        client.write('/dubbomesh/com.some.package.IHelloService/{0}:{1}'.format(get_ip(), port), weight)
        self.dubbo_channel_map = {}

    @gen.coroutine
    def handle_stream(self, stream, address):
        magic_number = yield stream.read_bytes(2)
        # gen_log.info('receive magic number success')
        if magic_number != '\xab\xcd':
            return
        interface_len = yield stream.read_bytes(4)
        # gen_log.info('get interface len: {0}'.format(repr(interface_len)))
        interface_len = bytes2int(interface_len)
        # gen_log.info('interface len: %d', interface_len)
        interface = yield stream.read_bytes(interface_len)
        # gen_log.info('get interface: [%s]', interface)
        method_len = yield stream.read_bytes(4)
        method_len = bytes2int(method_len)
        method = yield stream.read_bytes(method_len)
        pts_len = yield stream.read_bytes(4)
        pts_len = bytes2int(pts_len)
        pts = yield stream.read_bytes(pts_len)
        channel_key = (interface, method, pts)
        if channel_key in self.dubbo_channel_map:
            dubbo_client = self.dubbo_channel_map[channel_key]
        else:
            dubbo_client = DubboClient('localhost', 20880)

        def make_request(Id):
            ar = ActRequest()
            ar.interface = interface
            ar.method = method
            ar.parameter_types_string = pts
            ar.Id = Id
            return ar

        def make_dubbo_request(act_request):
            dr = DubboRequest()
            dr.Id = act_request.Id
            dr.interface_name = act_request.interface
            dr.method_name = act_request.method
            # dr.dubbo_version = ''
            # dr.version = ''
            dr.args = act_request.parameter
            return dr

        def make_act_response(dubbo_response):
            aresp = ActResponse()
            aresp.Id = dubbo_response.Id
            aresp.result = dubbo_response.result
            return aresp

        while True:
            try:
                Id = yield stream.read_bytes(4)
                request = make_request(bytes2int(Id))
                p_len = yield stream.read_bytes(4)
                request.parameter = yield stream.read_bytes(bytes2int(p_len))
                gen_log.info("get request %d %s", request.Id, request.parameter)

                def callback(dubbo_response):
                    gen_log.info("get dubbo_resp %d [%s] \nstream write act resp", dubbo_response.Id, dubbo_response.result)
                    stream.write(make_act_response(dubbo_response).encode_body())

                dubbo_client.fetch(make_dubbo_request(request), callback=callback)
            except StreamClosedError:
                break


class EchoServer(TCPServer):
    @gen.coroutine
    def handle_stream(self, stream, address):
        print 'another call'
        while True:
            try:
                data = yield stream.read_bytes(4)
                print 'receive data: [{0}]'.format(repr(data))
                # yield stream.write(data)
            except StreamClosedError:
                break

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
    define("port", default=30000, help="run on the given port", type=int)
    define("weight", default=1, help="weight", type=int)
    define("etcd", default="localhost", help="etcd hostname", type=str)
    tornado.options.parse_command_line()
    server = PAServer(port=options.port, weight=options.weight, etcd_host=options.etcd)
#     sockets = tornado.netutil.bind_sockets(options.port)
#     server.add_sockets(sockets)
    server.bind(options.port, backlog=2048)
    server.start(0)
    tornado.ioloop.IOLoop.current().start()
#     tornado.ioloop.IOLoop.instance().start()
