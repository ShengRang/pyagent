# coding: utf-8

import sys
import re

fname = sys.argv[1]

res = []

pattern = re.compile('malloc (?P<mal>\d+), give (\d+)')

with open(fname) as fp:
    for line in fp.readlines():
        sp = line.split()
        match = pattern.search(line)
        if match:
            res.append([int(match.group(1)), int(match.group(2))])

print 'total x_malloc number:', len(res)

res = sorted(res, key=lambda v: 1.0*v[0]/v[1])

print 'min: {0:.6f}({2}/{3}), max: {1:.6f}'.format(res[0][0]/res[0][1], res[-1][0]/res[-1][1], res[0][0], res[0][1])
print '------------'

print 'avg: {0:.3f}'.format(1.0 * sum(map(lambda x: x[0], res)) / sum(map(lambda x: x[1], res)))
