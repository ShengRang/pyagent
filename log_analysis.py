# coding: utf-8

import sys

fname = sys.argv[1]

res = []

with open(fname) as fp:
    for line in fp.readlines():
        sp = line.split()
        if line.startswith('[time_info]') and len(sp) > 4:
            try:
                res.append(int(sp[-1]) - int(sp[-3]))
            except ValueError:
                pass

res.sort()
print 'total request number:', len(res)
print '------------'
print 'min: {0:.6f}, max: {1:.6f}, avg: {2:.6f}'.format(res[0], res[1], sum(res)/len(res))
print '------------'

for r in [10, 15, 20, 30, 50, 60, 70, 80, 90, 95, 99]:
    v = res[min(int((r/100.0)*len(res)), len(res)-1)]
    print '{0}%: {1:.8f}'.format(r, v/1000000.0)
