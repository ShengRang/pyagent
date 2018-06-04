#!/bin/bash

ETCD_HOST=etcd
ETCD_PORT=2379
ETCD_URL=http://$ETCD_HOST:$ETCD_PORT

echo ETCD_URL = $ETCD_URL

if [[ "$1" == "consumer" ]]; then
  echo "Starting consumer agent..."
  pypy /root/dists/main.py --port=20000 > /dev/null
elif [[ "$1" == "provider-small" ]]; then
  echo "Starting small provider agent..."
  pypy /root/dists/pa.py --port=30000 > /dev/null
elif [[ "$1" == "provider-medium" ]]; then
  echo "Starting medium provider agent..."
  pypy /root/dists/pa.py --port=30000 > /dev/null
elif [[ "$1" == "provider-large" ]]; then
  echo "Starting large provider agent..."
  pypy /root/dists/pa.py --port=30000 > /dev/null
else
  echo "Unrecognized arguments, exit."
  exit 1
fi
