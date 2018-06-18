#ifndef BYTEBUF
#define BYTEBUF 0

#include <queue>
#include <vector>
#include <map>

typedef struct byte_buf_t{
    int read_idx;
    int write_idx;
    unsigned int size;
    char *buf;
}byte_buf_t;

struct byte_buf_t_cmp {
    bool operator() (const byte_buf_t*a, const byte_buf_t *b){
        return a->size < b->size;
    }
};

typedef struct{
    std::priority_queue<byte_buf_t*, std::vector<byte_buf_t*>, byte_buf_t_cmp> bufs;
}buf_pool_t;

byte_buf_t* create_buf(unsigned int size);
void free_buf(byte_buf_t *buf);
void pool_insert(buf_pool_t *pool, byte_buf_t *byte_buf);
void buf_flip(byte_buf_t *buf);
byte_buf_t* pool_malloc(buf_pool_t *pool, unsigned int size);
void pool_free(buf_pool_t *pool, byte_buf_t* buf);
void destroy_pool(buf_pool_t *pool);

extern buf_pool_t default_buf_pool;

// wrap malloc 的内存池

struct x_mem_pool {
    std::multimap<size_t, void*> ptr_pool;
    std::map<void*, size_t> ptr_size_map;
};

extern x_mem_pool default_x_mem_pool;

void *x_malloc(size_t size);
void x_free(void *ptr);

#endif
