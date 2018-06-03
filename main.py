#!/usr/bin/env python
# encoding: utf-8

import tornado
import tornado.web
import tornado.httpserver
import tornado.ioloop
import tornado.options
from tornado.options import define, options
from tornado import gen


def java_string_hashcode(s):
    h = 0
    for c in s:
        h = (31 * h + ord(c)) & 0xFFFFFFFF
    return ((h + 0x80000000) & 0xFFFFFFFF) - 0x80000000


class IndexHandler(tornado.web.RequestHandler):

    @gen.coroutine
    def post(self):
        parameter = self.get_argument('parameter')
        self.write(str(java_string_hashcode(parameter)))


class Application(tornado.web.Application):
    def __init__(self):
        settings = dict(
        )
        handlers = [
            (r'/', IndexHandler),
        ]
        tornado.web.Application.__init__(self, handlers, **settings)

if __name__ == '__main__':
    define("port", default=8888, help="run on the given port", type=int)
    tornado.options.parse_command_line()
    server = tornado.httpserver.HTTPServer(Application())
#     sockets = tornado.netutil.bind_sockets(options.port)
#     server.add_sockets(sockets)
    server.bind(options.port, backlog=2048)
    server.start(0)
    tornado.ioloop.IOLoop.current().start()
#     tornado.ioloop.IOLoop.instance().start()
