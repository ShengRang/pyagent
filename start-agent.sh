#!/bin/bash

ETCD_HOST=etcd
ETCD_PORT=2379
ETCD_URL=http://$ETCD_HOST:$ETCD_PORT

echo ETCD_URL = $ETCD_URL

if [[ "$1" == "consumer" ]]; then
  echo "Starting consumer agent..."
  java -jar -Xms1536M -Xmx1536M -Dserver.port=20000 -Detcd.url=http://127.0.0.1:2379 /root/dists/ca.jar
  # python /root/dists/ca.py --port=20000 --etcd=etcd > /root/logs/ca.log 2>&1
elif [[ "$1" == "provider-small" ]]; then
  echo "Starting small provider agent..."
  pypy /root/dists/pa.py --port=30000 --etcd=etcd --weight=2 > /root/logs/pas.log 2>&1
elif [[ "$1" == "provider-medium" ]]; then
  echo "Starting medium provider agent..."
  pypy /root/dists/pa.py --port=30000 --etcd=etcd --weight=3 > /root/logs/pam.log 2>&1
elif [[ "$1" == "provider-large" ]]; then
  echo "Starting large provider agent..."
  pypy /root/dists/pa.py --port=30000 --etcd=etcd --weight=3 > /root/logs/pal.log 2>&1
else
  echo "Unrecognized arguments, exit."
  exit 1
fi
