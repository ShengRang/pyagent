#!/bin/bash

ETCD_HOST=etcd
ETCD_PORT=2379
ETCD_URL=http://$ETCD_HOST:$ETCD_PORT

echo ETCD_URL = $ETCD_URL

if [[ "$1" == "consumer" ]]; then
  echo "Starting consumer agent..."
  nohup pypy /root/dists/main.py --port=20000 > /root/logs/consumer.log 2>&1 &
elif [[ "$1" == "provider-small" ]]; then
  echo "Starting small provider agent..."
  nohup pypy /root/dists/pa.py --port=30000 > /root/logs/p-s.log 2>&1 &
elif [[ "$1" == "provider-medium" ]]; then
  echo "Starting medium provider agent..."
  nohup pypy /root/dists/pa.py --port=30000 > /root/logs/p-m.log 2>&1 &
elif [[ "$1" == "provider-large" ]]; then
  echo "Starting large provider agent..."
  nohup pypy /root/dists/pa.py --port=30000 > /root/logs/p-l.log 2>&1 &
else
  echo "Unrecognized arguments, exit."
  exit 1
fi
