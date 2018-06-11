#ifndef DUBBO_CLIENT
#define DUBBO_CLIENT 0

#include <queue>
#include <map>
#include "bytebuf.h"

typedef struct dubbo_request {
    long long id;
    char *interface_name;
    char *method_name;
    char *dubbo_version;
    char *version;
    char *parameter_types_string;
    char *args;
    int two_way;
    int event;
}dubbo_request;

typedef struct dubbo_response {
    long long id;
    int data_len;
    char *result;
}dubbo_response;

struct dubbo_client;

typedef void (*dubbo_callback)(dubbo_response*, struct dubbo_client *client);

typedef struct dubbo_client{
    uv_tcp_t socket;
    uv_connect_t conn;
    uv_stream_t caller;
    byte_buf_t *buf;
    buf_pool_t buf_pool;
    int connected;
    std::queue<dubbo_request*> queue;
    std::map<long long, dubbo_callback> cbs;
    int read_state;
    dubbo_response dubbo_resp;
}dubbo_client;


void write_ll(char *buf, long long v);
void write_int(char *buf, int v);

uv_buf_t dubbo_request_encode(dubbo_request *request);

dubbo_client* create_dubbo_client(char *host, int port, uv_loop_t *io_loop);
void dubbo_fetch(dubbo_client *client, dubbo_request *request, dubbo_callback callback);

#endif