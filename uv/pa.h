#ifndef PA_H
#define PA_H

#include "dubbo_client.h"
#include "act.h"

struct dubbo_client;

typedef struct pa_server{
    dubbo_client *dubbo_client;
    uv_tcp_t socket;
//    byte_buf_t *buf;
    buf_pool_t buf_pool;
//    int header_state;
//    int has_header;
//    int body_state;
//    char *_interface;
//    int _interface_len;
//    char *method;
//    int method_len;
//    char *pts;
//    int pts_len;
//    act_request act;
}pa_server;

typedef struct stream_context {
    pa_server *server;
    byte_buf_t *buf;
    uv_tcp_t *channel;
    int header_state;
    int has_header;
    int body_state;
    char *_interface;
    int _interface_len;
    char *method;
    int method_len;
    char *pts;
    int pts_len;
    act_request act;
}stream_context;

#endif
