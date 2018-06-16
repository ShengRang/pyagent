#ifndef UTILS_H
#define UTILS_H 0

int bytes2int(char *buf);
long long bytes2ll(char *buf);
void write_ll(char *buf, long long v);
void write_int(char *buf, int v);
typedef unsigned int uint;
typedef unsigned int (*str_hash_func)(char *s, int len, unsigned int init);

#endif
