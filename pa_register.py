import logging
import logging.config
import socket
import time

import tornado.options
from tornado.options import define, options
from tornado.log import gen_log
import etcd

from utils import bytes2int, get_ip


define("port", default=30000, help="run on the given port", type=int)
define("weight", default=1, help="weight", type=int)
define("etcd", default="localhost", help="etcd hostname", type=str)
tornado.options.parse_command_line()
client = etcd.Client(host=options.etcd, port=2379)
client.write('/dubbomesh/com.alibaba.dubbo.performance.demo.provider.IHelloService/{0}:{1}'.format(get_ip(), options.port), options.weight)
gen_log.info('register with {0}:{1} [{2}]'.format(get_ip(), options.port, options.weight))

while True:
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    try:
        s.connect(('127.0.0.1', 20880))
        s.close()
        gen_log.info("connect to 20880 success")
        break
    except socket.error:
        gen_log.info("connect to 20880 fail, wait a second")
        time.sleep(0.2)
        pass
