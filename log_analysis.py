# coding: utf-8

import sys

fname = sys.argv[1]

res = []

with open(fname) as fp:
    for line in fp.readlines():
        sp = line.split(', ')
        if line.startswith('[') and len(sp) > 4:
            res.append(float(sp[-1]))

res.sort()
print 'total request number:', len(res)

for r in [10, 15, 20, 30, 50, 60, 70, 80, 90, 95, 99]:
    v = res[int((r/100.0)*len(res))]
    print '{0}%: {1:.8f}'.format(r, v)
