#ifndef DUBBO_CLIENT
#define DUBBO_CLIENT 0

#include <queue>
#include <map>
#include "bytebuf.h"
#include "pa.h"

typedef struct dubbo_request {
    long long id;
    char *interface_name;           int interface_len;
    char *method_name;              int method_len;
    char *dubbo_version;            int dubbo_version_len;
    char *version;                  int version_len;
    char *parameter_types_string;   int pts_len;
    char *args;                     int args_len;
    int two_way;
    int event;
}dubbo_request;

typedef struct dubbo_response {
    long long id;
    int data_len;
    char *result;
}dubbo_response;

struct dubbo_client;
struct stream_context;

typedef void (*dubbo_callback)(dubbo_response*, struct stream_context *context);

typedef struct dubbo_client{
    uv_tcp_t socket;
    uv_connect_t conn;
    void *caller;
    byte_buf_t *buf;
    buf_pool_t buf_pool;
    int connected;
    std::queue<dubbo_request*> queue;
    std::map<long long, dubbo_callback> cbs;
    std::map<long long, stream_context*> contexts;
    int read_state;
    dubbo_response dubbo_resp;
}dubbo_client;


void write_ll(char *buf, long long v);
void write_int(char *buf, int v);

uv_buf_t dubbo_request_encode(dubbo_request *request);

dubbo_client* create_dubbo_client(char *host, int port, uv_loop_t *io_loop);
void dubbo_fetch(dubbo_client *client, dubbo_request *request, dubbo_callback callback, stream_context *context);

#endif