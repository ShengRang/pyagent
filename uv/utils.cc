int bytes2int(char *buf) {
    return ((buf[0]&0xff) << 24) | ((buf[1]&0xff) << 16) | ((buf[2] & 0xff) << 8) | (buf[3] & 0xff);
}

long long bytes2ll(char *buf) {
    int lo, hi;
    hi = bytes2int(buf);
    lo = bytes2int(buf+4);
    return ((long long)hi << 32) + lo;
}

void write_ll(char *buf, long long v){
    buf[0] = (v >> 56) & 0xff;
    buf[1] = (v >> 48) & 0xff;
    buf[2] = (v >> 40) & 0xff;
    buf[3] = (v >> 32) & 0xff;
    buf[4] = (v >> 24) & 0xff;
    buf[5] = (v >> 16) & 0xff;
    buf[6] = (v >> 8) & 0xff;
    buf[7] = v & 0xff;
}

void write_int(char *buf, int v){
    buf[0] = (v >> 24) & 0xff;
    buf[1] = (v >> 16) & 0xff;
    buf[2] = (v >> 8) & 0xff;
    buf[3] = v & 0xff;
}