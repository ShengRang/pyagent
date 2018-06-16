#ifndef CA_H
#define CA_H

#include <uv.h>
#include <map>

#include "http-parser/http_parser.h"
#include "bytebuf.h"
#include "pa_client.h"
#include "lb.h"

typedef unsigned int uint;
typedef unsigned int (*str_hash_func)(char *s, int len, unsigned int init);

typedef struct ca_server {
    uv_tcp_t server;
    http_parser_settings settings;
    uv_loop_t *io_loop;
    buf_pool_t buf_pool;
    etcd_lb lb;
    std::map<uint, pa_client*> pa_client_maps[10];
    str_hash_func ca_hash;
    uint _interface_hash;
    uint _method_hash;
    uint _pts_hash;
}ca_server;

typedef struct h_context {
    ca_server *server;
    uv_tcp_t *stream;           // ->data == ctx
    http_parser *parser;        // parser->data === &h_context
    int next_new_req;
    byte_buf_t *buf;
    buf_pool_t buf_pool;
}h_context;

#endif
