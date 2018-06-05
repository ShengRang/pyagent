#!/usr/bin/env python
# encoding: utf-8

import random

import tornado
import tornado.web
import tornado.httpserver
import tornado.ioloop
import tornado.options
from tornado.options import define, options
from tornado.web import asynchronous
from tornado import gen
import etcd

from act import ActRequest, ActResponse
from pa_client import PAClient


def java_string_hashcode(s):
    h = 0
    for c in s:
        h = (31 * h + ord(c)) & 0xFFFFFFFF
    return ((h + 0x80000000) & 0xFFFFFFFF) - 0x80000000


class EndPoints(object):

    def __init__(self):
        eclient = etcd.Client(host=options.etcd, port=2379)
        end_points = []
        # end_points = [tuple(e.key[42:].split(':')) + (int(e.value),) for e in eclient.read('/dubbomesh/com.some.package.IHelloService').children]
        for e in eclient.read('/dubbomesh/com.some.package.IHelloService').children:
            s = e.key[42:].split(':')
            end_points.append((s[0], int(s[1]), int(e.value)))
        self.end_points = sorted(end_points, key=lambda pair: -pair[2])

    def choice(self):
        return random.choice(self.end_points)[:2]


class IndexHandler(tornado.web.RequestHandler):

    pa_client_map = {}
    alloc = 0

    @asynchronous
    def post(self):
        host, port = end_points.choice()
        if host in IndexHandler.pa_client_map:
            client = IndexHandler.pa_client_map[host]
        else:
            client = PAClient(host, port)
            IndexHandler.pa_client_map[host] = client
        request = ActRequest()
        print 'alloc: ', IndexHandler.alloc
        request.Id = IndexHandler.alloc
        IndexHandler.alloc += 1
        request.interface = self.get_argument('interface').encode('utf-8')
        request.method = self.get_argument('method').encode('utf-8')
        request.parameter_types_string = self.get_argument('parameterTypesString').encode('utf-8')
        request.parameter = self.get_argument('parameter').encode('utf-8')
        client.fetch(request, self._callback)
        # self.write(str(java_string_hashcode(parameter)))

    def _callback(self, act_response):
        print 'get act resp', act_response.Id, act_response.result
        self.write(act_response.result.split()[1])
        self.finish()


class Application(tornado.web.Application):
    def __init__(self):
        settings = dict()
        handlers = [
            (r'/', IndexHandler),
        ]
        tornado.web.Application.__init__(self, handlers, **settings)

if __name__ == '__main__':
    define("port", default=20000, help="run on the given port", type=int)
    define("etcd", default="localhost", help="etcd hostname", type=str)
    tornado.options.parse_command_line()
    end_points = EndPoints()
    server = tornado.httpserver.HTTPServer(Application())
#     sockets = tornado.netutil.bind_sockets(options.port)
#     server.add_sockets(sockets)
    server.bind(options.port, backlog=2048)
    server.start(0)
    tornado.ioloop.IOLoop.current().start()
#     tornado.ioloop.IOLoop.instance().start()
