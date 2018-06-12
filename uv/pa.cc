#include <uv.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "dubbo_client.h"
#include "bytebuf.h"
#include "pa.h"
#include "utils.h"

#define MIN(a,b) ((a)<(b)?(a):(b))
#define MAX(a,b) ((a)>(b)?(a):(b))

uv_loop_t io_loop;
pa_server server;

stream_context *create_pa_context(pa_server *server, uv_tcp_t *channel) {
    stream_context *res = (stream_context*)malloc(sizeof(stream_context));
    res->server = server;
    res->header_state = res->body_state = res->has_header = 0;
    res->buf = NULL;
    res->channel = channel;
    printf("[create_pa_context]: finish create context, p: %p\n", res);
    return res;
}

void _p_alloc_cb(uv_handle_t *handle, size_t suggested_size, uv_buf_t *buf){
//    printf("[alloc]\n");
    int remain = 0;
    stream_context* context = (stream_context*)(handle->data);
    pa_server *server = context->server;
//    printf("[alloc]: context p: %p, server p: %p\n", context, server);
    if(context->buf){
        remain = context->buf->size - context->buf->write_idx;
    }
    printf("[alloc]: remain: %d\n", remain);
    if(remain < suggested_size/3) {
        printf("server no enough buffer, alloc new and push old, buf_pool size: %d\n", server->buf_pool.bufs.size());
        byte_buf_t *new_buf = pool_malloc(&server->buf_pool, suggested_size);
        printf("new buf address: %p\n", new_buf);
        if(context->buf) {
            int ri = context->buf->read_idx;
            int wi = context->buf->write_idx;
            printf("will copy %d bytes\n", wi-ri);
            for(int i=ri; i<wi; i++) {
                new_buf->buf[i-ri] = context->buf->buf[i];
                new_buf->write_idx++;
            }
            pool_free(&server->buf_pool, context->buf);
        }
        context->buf = new_buf;
    }
    buf->base = context->buf->buf + context->buf->write_idx;
    buf->len = context->buf->size - context->buf->write_idx;
//    printf("[alloc]: finish cb\n");
}

act_response* act_response_from_dubbo(dubbo_response *dubbo_resp){
    act_response *res = (act_response*)malloc(sizeof(act_response));
    res->id = dubbo_resp->id;
    res->data_len = dubbo_resp->data_len;
    res->result = dubbo_resp->result;
    return res;
}

void act_write_cb(uv_write_t* req, int status) {
    printf("[act_write_cb]: status: %d[%s]\n", status, uv_strerror(status));
    if(status) {
        printf("[act_write_cb]: status: %d[%s]\n", status, uv_strerror(status));
//        fprintf(stderr, "write cb error %s\n", uv_strerror(status));
    }
    free(req);
}

int act_response_data_length(act_response *resp) {
    return 8 + resp->data_len;
}

char* encode_act_response(act_response *resp) {
    char *res = (char*)malloc(resp->data_len + 4 + 4);
    write_int(res, resp->id);
    write_int(res+4, resp->data_len);
    strncpy(res+8, resp->result, resp->data_len);
    return res;
}

void _dubbo_callback(dubbo_response *resp, stream_context *context) {
    printf("[test cb]: get resp id: %lld, data len: %d, data: [...]\n", resp->id, resp->data_len); //, resp->data_len, resp->result);
    printf("caller p: %p, server p: %p\n", context->server, &server);
    act_response *act_resp = act_response_from_dubbo(resp);
    pa_server *server = (pa_server*)context->server;
    uv_buf_t *buf = (uv_buf_t*)malloc(sizeof(uv_buf_t));
    buf->base = encode_act_response(act_resp);
    buf->len = act_response_data_length(act_resp);
    uv_write_t *w_req = (uv_write_t*)malloc(sizeof(uv_write_t));
    int ret = uv_write(w_req, (uv_stream_t*)context->channel, buf, 1, act_write_cb);
    printf("[_dubbo_callback]: write 1 buf to %p, ret: %d\n", context->channel, ret);
    if(ret) {
        printf("act write error: %s-%s\n", uv_err_name(ret), uv_strerror(ret));
    }
//    free(resp->result);
}

dubbo_request* dubbo_request_from_act(act_request *act){
    dubbo_request *res = (dubbo_request*)malloc(sizeof(dubbo_request));
    res->parameter_types_string = act->parameter_types_string;
    res->pts_len = act->pts_len;

    res->id = act->id;

    res->interface_name = act->_interface;
    res->interface_len = act->interface_len;

    res->method_name = act->method;
    res->method_len = act->method_len;

    res->args = act->parameter;
    res->args_len = act->p_len;

    res->dubbo_version = "2.0.1";
    res->dubbo_version_len = 5;

    res->two_way = 1;
    return res;
}

void on_read(uv_stream_t *client, ssize_t nread, const uv_buf_t *uv_buf) {
    if (nread < 0) {
        if (nread != UV_EOF)
            fprintf(stderr, "Read error %s\n", uv_err_name(nread));
        uv_close((uv_handle_t*) client, NULL);
    } else if (nread > 0) {
        printf("[p_read_cb]: client p: %p, nread: %ld\n", client, nread);
        stream_context *context = (stream_context*)(client->data);
        pa_server *server = context->server;
        context->buf->write_idx += nread;
        printf("[p_read_cb]: add widx to %d, %d\n", context->buf->write_idx, context->buf->size);
        byte_buf_t *buf = context->buf;
        while(buf->read_idx < buf->write_idx) {
            printf("[p_read_cb]: in turn: ridx: %d, widx: %d, hs: %d, bs: %d, hh: %d\n", buf->read_idx, buf->write_idx, context->header_state, context->body_state, context->has_header);
            if(!context->has_header){
                // read head
                if(context->header_state == 0) {
                    if(buf->write_idx - buf->read_idx >= 2) {
                        if ((buf->buf[buf->read_idx] & 0xff) == 0xab && (buf->buf[buf->read_idx + 1] & 0xff) == 0xcd) {
                            printf("read act magic number success!\n");
                            buf->read_idx += 2;
                            context->header_state++;
                        }
                    } else {
                        break;
                    }
                }
                if(context->header_state == 1) {
                    if(buf->write_idx - buf->read_idx >= 4) {
                        context->_interface_len = bytes2int(buf->buf + buf->read_idx);
                        buf->read_idx += 4;
                        context->header_state++;
                    } else {
                        break;
                    }
                }
                if(context->header_state == 2) {
                    // 注意并没有字符串的尾0
                    if (buf->write_idx - buf->read_idx >= context->_interface_len) {
                        context->_interface = (char *) malloc(sizeof(char) * context->_interface_len);
                        strncpy(context->_interface, buf->buf + buf->read_idx, context->_interface_len);
                        buf->read_idx += context->_interface_len;
                        context->header_state++;
                    } else {
                        break;
                    }
                }
                if(context->header_state == 3) {
                    if (buf->write_idx - buf->read_idx >= 4) {
                        context->method_len = bytes2int(buf->buf + buf->read_idx);
                        buf->read_idx += 4;
                        context->header_state++;
                    } else {
                        break;
                    }
                }
                if(context->header_state == 4) {
                    if (buf->write_idx - buf->read_idx >= context->method_len) {
                        context->method = (char *) malloc(context->method_len);
                        strncpy(context->method, buf->buf + buf->read_idx, context->method_len);
                        buf->read_idx += context->method_len;
                        context->header_state++;
                    } else {
                        break;
                    }
                }
                if(context->header_state == 5) {
                    if(buf->write_idx - buf->read_idx >= 4) {
                        context->pts_len = bytes2int(buf->buf + buf->read_idx);
                        buf->read_idx += 4;
                        context->header_state++;
                    } else {
                        break;
                    }
                }
                if(context->header_state == 6) {
                    if (buf->write_idx - buf->read_idx >= context->pts_len) {
                        context->pts = (char *) malloc(context->pts_len);
                        strncpy(context->pts, buf->buf + buf->read_idx, context->pts_len);
                        buf->read_idx += context->pts_len;
                        context->header_state++;
                        context->has_header = 1;
                    } else {
                        break;
                    }
                }
            }
            if(context->has_header) {    // 相同信息已获取
                if(context->body_state == 0) {
                    if (buf->write_idx-buf->read_idx >= 4) {
                        context->act.id = bytes2int(buf->buf + buf->read_idx);
                        buf->read_idx += 4;
                        context->body_state++;
                    } else {
                        break;
                    }
                }
                if(context->body_state == 1) {
                    if(buf->write_idx - buf->read_idx >= 4) {
                        context->act.p_len = bytes2int(buf->buf + buf->read_idx);
                        buf->read_idx += 4;
                        context->body_state++;
                    } else {
                        break;
                    }
                }
                if(context->body_state == 2) {
                    printf("[p_read_cb]: need to read p_len: %d\n", context->act.p_len);
                    if(buf->write_idx - buf->read_idx >= context->act.p_len) {
                        context->act.parameter = (char *) malloc(context->act.p_len);
                        strncpy(context->act.parameter, buf->buf + buf->read_idx, context->act.p_len);
                        buf->read_idx += context->act.p_len;
                        context->body_state = 0;
                        context->act._interface = context->_interface;
                        context->act.interface_len = context->_interface_len;
                        context->act.method = context->method;
                        context->act.method_len = context->method_len;
                        printf("method len: %d\n", context->method_len);
                        context->act.parameter_types_string = context->pts;
                        context->act.pts_len = context->pts_len;
                        printf("act Id: %d, ridx: %d, widx: %d, size: %d  args: [...]\n", context->act.id,
                               buf->read_idx, buf->write_idx, buf->size);
                        // context->act.p_len, context->act.parameter);
                        dubbo_fetch(server->dubbo_client, dubbo_request_from_act(&context->act), &_dubbo_callback,
                                    context);
                    } else {
                        break;
                    }
                }
            }
        }
        printf("[pa_on_read]: leave while, ridx: %d, widx: %d\n", context->buf->read_idx, context->buf->write_idx);
    }
}

void on_conn(uv_stream_t *server, int status){
    printf("[conn] server p: %p\n", server);
    if(status < 0){
        printf("New conn error!");
    }
    uv_tcp_t *client = (uv_tcp_t*)malloc(sizeof(uv_tcp_t));
    printf("malloc client p: [%p]\n", client);
    client->data = create_pa_context((pa_server*)server->data, client);
    uv_tcp_init(&io_loop, client);
    if(uv_accept(server, (uv_stream_t*) client) == 0) {
        uv_read_start((uv_stream_t*)client, _p_alloc_cb, on_read);
    } else {
        uv_close((uv_handle_t*)client, NULL);
    }
}

void pa_server_init(pa_server *server, uv_loop_t *ioloop){
    server->socket.data = server;
//    server->buf = NULL;
    server->buf_pool = default_buf_pool;
//    server->header_state = server->body_state = server->has_header = 0;
    server->dubbo_client = create_dubbo_client("127.0.0.1", 20880, ioloop);
    server->dubbo_client->caller = (void*)server;
}

int main(int argc, char *argv[]) {
    printf("[main]: server p: %p\n", &server);
    uv_loop_init(&io_loop);
    pa_server_init(&server, &io_loop);
    uv_tcp_init(&io_loop, &server.socket);
    struct sockaddr_in addr;

    uv_ip4_addr("0.0.0.0", 30000, &addr);
    uv_tcp_bind(&server.socket, (const struct sockaddr*)&addr, 0);
    int r = uv_listen((uv_stream_t*)&server.socket, 1024, on_conn);
    uv_run(&io_loop, UV_RUN_DEFAULT);
    return 0;
}
