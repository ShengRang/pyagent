#!/bin/bash

ETCD_HOST=etcd
ETCD_PORT=2379
ETCD_URL=http://$ETCD_HOST:$ETCD_PORT

echo ETCD_URL = $ETCD_URL

if [[ "$1" == "consumer" ]]; then
  echo "Starting consumer agent..."
  java -Xms4166M -Xmx4166M -jar -Dserver.port=20000 -Detcd.url=http://etcd:2379 /root/dists/ca.jar > /root/logs/ca.log 2>&1
  # pypy /root/dists/ca.py --port=20000 --etcd=etcd > /root/logs/ca.log 2>&1
elif [[ "$1" == "provider-small" ]]; then
  echo "Starting small provider agent..."
  # pypy /root/dists/pa.py --port=30000 --etcd=etcd --weight=14 > /root/logs/pa.log 2>&1
  pypy /root/dists/pa_register.py  --port=30000 --etcd=etcd --weight=15
  /root/dists/a.out > /root/logs/pas.log 2>&1
elif [[ "$1" == "provider-medium" ]]; then
  echo "Starting medium provider agent..."
  pypy /root/dists/pa_register.py  --port=30000 --etcd=etcd --weight=24
  /root/dists/a.out > /root/logs/pam.log 2>&1
  # pypy /root/dists/pa.py --port=30000 --etcd=etcd --weight=25 > /root/logs/pa.log 2>&1
elif [[ "$1" == "provider-large" ]]; then
  echo "Starting large provider agent..."
  pypy /root/dists/pa_register.py  --port=30000 --etcd=etcd --weight=24
  /root/dists/a.out > /root/logs/pal.log 2>&1
  # pypy /root/dists/pa.py --port=30000 --etcd=etcd --weight=25 > /root/logs/pa.log 2>&1
else
  echo "Unrecognized arguments, exit."
  exit 1
fi
