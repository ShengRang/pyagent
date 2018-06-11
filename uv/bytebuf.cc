#include "bytebuf.h"

#include <stdlib.h>
#include <stdio.h>

byte_buf_t* create_buf(unsigned int size){
    byte_buf_t* buf = (byte_buf_t*)malloc(sizeof(byte_buf_t));
    buf->buf = (char*)malloc(size);
    buf->read_idx = buf->write_idx = 0;
    buf->size = size;
    return buf;
}

void free_buf(byte_buf_t *buf) {
    free(buf->buf);
    free(buf);
}

void pool_insert(buf_pool_t *pool, byte_buf_t *byte_buf){
    pool->bufs.push(byte_buf);
}

void buf_flip(byte_buf_t *buf){
    buf->write_idx = buf->read_idx = 0;
}

byte_buf_t* pool_malloc(buf_pool_t *pool, unsigned int size){
    if(pool->bufs.empty() || pool->bufs.top()->size < size) {
        byte_buf_t *buf = create_buf(size);
        // pool->bufs.push(buf);
        return buf;
    } else {
        byte_buf_t *buf = pool->bufs.top();
        pool->bufs.pop();
        buf_flip(buf);
        return buf;
    }
}

void pool_free(buf_pool_t *pool, byte_buf_t* buf){
    pool->bufs.push(buf);
}

void destroy_pool(buf_pool_t *pool) {
    while(!pool->bufs.empty()) {
        byte_buf_t *buf = pool->bufs.top();
        free_buf(buf);
        pool->bufs.pop();
    }
    // free(pool);
}

buf_pool_t default_buf_pool;
