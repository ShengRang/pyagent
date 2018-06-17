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

try:
    import multiprocessing
except ImportError:
    # Multiprocessing is not available on Google App Engine.
    multiprocessing = None


def cpu_count():
    """Returns the number of processors on this machine."""
    if multiprocessing is None:
        return 1
    try:
        return multiprocessing.cpu_count()
    except NotImplementedError:
        pass
    try:
        return os.sysconf("SC_NPROCESSORS_CONF")
    except (AttributeError, ValueError):
        pass
    gen_log.error("Could not detect number of processors; assuming 1")
    return 1

eclient = etcd.Client(host=options.etcd, port=2379)
end_points = []
uv_argv = []
for e in eclient.read('/dubbomesh/com.alibaba.dubbo.performance.demo.provider.IHelloService').children:
    s = e.key[69:].split(':')
    gen_log.info('get endpoint: {0}, {1}, {2}'.format(s[0], s[1], e.value))
    end_points.append((s[0], int(s[1]), int(e.value)))
    uv_argv.extend([s[0], s[1], e.value])

print 'cpu count:', cpu_count()

# instance_count = 2
#
# for i in range(instance_count-1):
#     pid = os.fork()
#     if pid == 0:
#         os.execv(options.ca_path, uv_argv)

os.execv(options.ca_path, uv_argv)
