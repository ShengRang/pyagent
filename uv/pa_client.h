#ifndef PA_CLIENT_H
#define PA_CLIENT_H 0

#include <queue>
#include <map>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <uv.h>

#include "bytebuf.h"
#include "act.h"

struct h_context;

typedef void (*pa_callback)(act_response *, h_context *);

typedef struct act_reuse_key{
    char *interface;    int interface_len;
    char *method;       int method_len;
    char *pts;          int pts_len;
}act_reuse_key;

typedef struct pa_client {
    uv_tcp_t stream;
    uv_connect_t conn;
    act_reuse_key *key;         // TODO 检查内存回收
    byte_buf_t *buf;
    buf_pool_t buf_pool;
    int connected;
    std::queue<act_request*> que;
    std::map<int, pa_callback> callbacks;
    std::map<int, h_context*> contexts;
    int read_state;
    act_response a_resp;
}pa_client;

pa_client* create_pa_client(char *host, int port, act_reuse_key *key, uv_loop_t *io_loop);
void pa_client_fetch(pa_client *client, act_request *req, pa_callback cb, h_context *context);

#endif
