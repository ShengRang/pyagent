# coding: utf-8

import os

import tornado.options
from tornado.options import define, options
import etcd
from tornado.log import gen_log

define("port", default=20000, help="run on the given port", type=int)
define("etcd", default="localhost", help="etcd hostname", type=str)
define("ca_path", default="./uv/ca.out", help="path of uv ca", type=str)
tornado.options.parse_command_line()

eclient = etcd.Client(host=options.etcd, port=2379)
end_points = []
uv_argv = []
for e in eclient.read('/dubbomesh/com.alibaba.dubbo.performance.demo.provider.IHelloService').children:
    s = e.key[69:].split(':')
    gen_log.info('get endpoint: {0}, {1}, {2}'.format(s[0], s[1], e.value))
    end_points.append((s[0], int(s[1]), int(e.value)))
    uv_argv.extend([s[0], s[1], e.value])

os.execv(options.ca_path, uv_argv)
