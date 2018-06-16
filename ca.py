#!/usr/bin/env python
# encoding: utf-8

import random
import bisect
import logging
import os
import logging.config

import tornado
import tornado.web
import tornado.httpserver
import tornado.ioloop
import tornado.options
from tornado.options import define, options
from tornado.web import asynchronous
from tornado.log import gen_log
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
        for e in eclient.read('/dubbomesh/com.alibaba.dubbo.performance.demo.provider.IHelloService').children:
            s = e.key[69:].split(':')
            gen_log.info('get endpoint: {0}, {1}, {2}'.format(s[0], s[1], e.value))
            end_points.append((s[0], int(s[1]), int(e.value)))
        self.end_points = sorted(end_points, key=lambda pair: -pair[2])
        rate = map(lambda u: u[2], self.end_points)
        for i, v in enumerate(rate):
            if i > 0:
                rate[i] = rate[i-1] + rate[i]
        # gen_log.info('the rate: {0}'.format(rate))
        self.rate = rate
        self.prev = 0
        self.max_sum = self.rate[-1]
        self.cnt_map = [0 for _ in self.rate]

    def choice(self):
        # return random.choice(self.end_points)[:2]
        idx = bisect.bisect_left(self.rate, self.prev+1)
        self.cnt_map[idx] += 1
        # gen_log.info("cnt_map: {0}".format(self.cnt_map))
        res = self.end_points[idx][:2]
        self.prev = (self.prev + 1) % self.max_sum
        return res


class IndexHandler(tornado.web.RequestHandler):

    pa_client_map = {}
    alloc = 0

    @asynchronous
    def post(self):
        print self.request.connection.context.address
        # parameter = self.get_argument('parameter').encode('utf-8')
        # self.write(str(java_string_hashcode(parameter)))
        # self.finish()
        # return
        host, port = end_points.choice()
        # gen_log.info('chose {0}:{1}'.format(host, port))
        if host in IndexHandler.pa_client_map:
            client = IndexHandler.pa_client_map[host]
        else:
            # print host, port, 'not in the pa_client_map'
            # gen_log.info('{0}:{1} not in pa_client_map'.format(host, port))
            # clients = [PAClient(host, port) for _ in range(4)]
            client = PAClient(host, port)
            IndexHandler.pa_client_map[host] = client
        request = ActRequest()
        # print 'alloc: ', IndexHandler.alloc
        # request.Id = IndexHandler.alloc
        request.Id = random.randint(1, 2100000000)
        IndexHandler.alloc += 1
        request.interface = self.get_argument('interface').encode('utf-8')
        request.method = self.get_argument('method').encode('utf-8')
        request.parameter_types_string = self.get_argument('parameterTypesString').encode('utf-8')
        request.parameter = self.get_argument('parameter').encode('utf-8')
        # gen_log.info("Id: {0}, arg: {1}".format(request.Id, request.parameter))
        client.fetch(request, self._callback)

    def _callback(self, act_response):
        # print 'get act resp', act_response.Id, act_response.result
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
    define("port", default=20000, help="run on the given port", type=int)
    define("etcd", default="localhost", help="etcd hostname", type=str)
    tornado.options.parse_command_line()
    end_points = EndPoints()
    server = tornado.httpserver.HTTPServer(Application())
#     sockets = tornado.netutil.bind_sockets(options.port)
#     server.add_sockets(sockets)
    server.bind(options.port, backlog=2048)
    server.start(1)
    random.seed(os.getpid())
    tornado.ioloop.IOLoop.current().start()
#     tornado.ioloop.IOLoop.instance().start()
