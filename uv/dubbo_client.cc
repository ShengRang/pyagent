#include <stdio.h>
#include <uv.h>
#include <stdlib.h>
#include <string.h>
#include <algorithm>
#include <map>

#include "dubbo_client.h"
#include "bytebuf.h"
#include "utils.h"

#define MIN(a,b) ((a)<(b)?(a):(b))
#define MAX(a,b) ((a)>(b)?(a):(b))

#define FK 0

typedef struct dubbo_encode_wcb_context{
    uv_buf_t *bufs;
    int bcnt;
}dubbo_encode_wcb_context;

uv_buf_t dubbo_request_encode(dubbo_request *request) {
    // int dubbo_version_len = strlen(request->dubbo_version);
    int dubbo_version_len = request->dubbo_version_len;
    // int interface_name_len = strlen(request->interface_name);
    int interface_name_len = request->interface_len;
//    int method_name_len = strlen(request->method_name);
    int method_name_len = request->method_len;
    // int parameter_types_string_len = strlen(request->parameter_types_string);
    int parameter_types_string_len = request->pts_len;
    int args_len = request->args_len;
    // int args_len = strlen(request->args);
    // '{"path":"com.alibaba.dubbo.performance.demo.provider.IHelloService"}\n'
    int f_len = 69;
    int total_len = 4 + 8 + 4 + (dubbo_version_len + 1) + (interface_name_len + 1) + 5 + (parameter_types_string_len + 1)
                    + (args_len + 1) + (f_len) + 10 + (method_name_len + 1);
    char *buffer = (char*)malloc(total_len);                        // TODO: free
//    printf("%d, %d, %d, %d, %d, %d\n", dubbo_version_len, interface_name_len, method_name_len, parameter_types_string_len, args_len, f_len);
//    printf("[dubbo encode]: total len: %d\n", total_len);
    int pos = 0;
    buffer[0] = 0xda; buffer[1] = 0xbb; buffer[2] = 0xc6; buffer[3] = 0x00; pos += 4;

    write_ll(buffer+pos, request->id);  pos += 8;
    write_int(buffer+pos, total_len-16); pos += 4;

    buffer[pos++] = '"';
    for(int i=0; i<dubbo_version_len; i++) {
        buffer[pos++] = request->dubbo_version[i];
    }
    buffer[pos++] = '"';
    buffer[pos++] = '\n';

    buffer[pos++] = '"';
    for(int i=0; i<interface_name_len; i++) {
        buffer[pos++] = request->interface_name[i];
    }
    buffer[pos++] = '"';
    buffer[pos++] = '\n';

    buffer[pos++] = 'n'; buffer[pos++] = 'u'; buffer[pos++] = 'l'; buffer[pos++] = 'l'; buffer[pos++] = '\n';

    buffer[pos++] = '"';
    for(int i=0; i<method_name_len; i++)
        buffer[pos++] = request->method_name[i];
    buffer[pos++] = '"';
    buffer[pos++] = '\n';

    buffer[pos++] = '"';
    for(int i=0; i<parameter_types_string_len; i++)
        buffer[pos++] = request->parameter_types_string[i];
    buffer[pos++] = '"';
    buffer[pos++] = '\n';

    buffer[pos++] = '"';
    for(int i=0; i<args_len; i++)
        buffer[pos++] = request->args[i];
    buffer[pos++] = '"';
    buffer[pos++] = '\n';

    strncpy(buffer+pos, "{\"path\":\"com.alibaba.dubbo.performance.demo.provider.IHelloService\"}\n", 69);
    uv_buf_t res = uv_buf_init(buffer, total_len);

//    printf("[dubbo_encode]: ");
//    for(int i=0; i<16; i++)
//        printf("%x ", buffer[i] & 0xff);
    // printf("next: [%.*s]\n", total_len-16, buffer+16);

    return res;
}

void dubbo_write_cb(uv_write_t* req, int status) {
    dubbo_encode_wcb_context *ctx = (dubbo_encode_wcb_context*)req->data;
    for(int i=0; i<ctx->bcnt; i++) {
//        printf("[dubbo_write_cb]: free ctx->bufs[%d].base: %p\n", i, ctx->bufs[i].base);
        free(ctx->bufs[i].base);
    }
//    printf("[dubbo_write_cb]: free ctx->bufs: %p\n", ctx->bufs);
    free(ctx->bufs);
//    printf("[dubbo_write_cb]: free ctx: %p\n", ctx);
    free(ctx);
//    printf("[dubbo_write_cb]: status: %d\n", status);
    if(status) {
        fprintf(stderr, "write cb error %s\n", uv_strerror(status));
    }
//    printf("[dubbo_write_cb]: free write_req: %p\n", req);
    free(req);
}

void free_dubbo_request(dubbo_request *request) {
//    printf("o i will free memory\n");
//    printf("free inter: %p\n", request->interface_name);
//    printf("free method: %p\n", request->method_name);
//    printf("free pts: %p\n", request->parameter_types_string);
//    printf("free args: %p\n", request->args);
//    printf("free dubbo_request: %p\n", request);
//    free(request->interface_name);
//    free(request->method_name);
//    free(request->parameter_types_string);
    free(request->args);
    free(request);
//    printf("finish free\n");
}

void handle_queue(dubbo_client *client) {
    if(!client->connected) {
//        printf("[handle_queue]: connection not ready\n");
        return;
    }
    uv_buf_t *bufs = (uv_buf_t*)malloc(sizeof(uv_buf_t)*client->queue.size() + FK);      // TODO: free bufs
    int bcnt = 0;
    while(!client->queue.empty()) {
        dubbo_request *dreq = client->queue.front(); client->queue.pop();
        bufs[bcnt++] = dubbo_request_encode(dreq);                              // TODO: 回收内存
        // free_dubbo_request(dreq);
    }
    if(bcnt <= 0) {
//        printf("bcnt <= 0, break\n");
        free(bufs);
        return;
    }
    uv_write_t *w_req = (uv_write_t*)malloc(sizeof(uv_write_t) + FK);
    dubbo_encode_wcb_context* ctx = (dubbo_encode_wcb_context *)malloc(sizeof(dubbo_encode_wcb_context) + FK);
    ctx->bufs = bufs; ctx->bcnt = bcnt;
    w_req->data = ctx;
    int ret = uv_write(w_req, (uv_stream_t*) &(client->socket), bufs, bcnt, dubbo_write_cb);
//    printf("dubbo write %d bufs to client->socket, ret: %d\n", bcnt, ret);
    if(ret) {
        printf("dubbo write error: %s-%s\n", uv_err_name(ret), uv_strerror(ret));
    }
}

void _d_alloc_cb(uv_handle_t *handle, size_t suggested_size, uv_buf_t *buf){
    int remain = 0;
    dubbo_client *client = (dubbo_client*)(handle->data);
    if(client->buf){
       remain = client->buf->size - client->buf->write_idx;
    }
//    suggested_size = 2048;
    if(remain < suggested_size/3) {
//        printf("[d_alloc_cb]: no enough buffer, alloc new and push old, buf_pool size: %d\n", client->buf_pool.bufs.size());
        byte_buf_t *new_buf = pool_malloc(&client->buf_pool, suggested_size);
//        printf("[d_alloc_cb]: new buf address: %p\n", new_buf);
        if(client->buf) {
            int ri = client->buf->read_idx;
            int wi = client->buf->write_idx;
            printf("[d_alloc_cb]: will copy %d bytes\n", wi-ri);
            for(int i=ri; i<wi; i++) {
                new_buf->buf[i-ri] = client->buf->buf[i];
                new_buf->write_idx++;
            }
            pool_free(&client->buf_pool, client->buf);
        }
        client->buf = new_buf;
    }
    buf->base = client->buf->buf + client->buf->write_idx;
    buf->len = client->buf->size - client->buf->write_idx;
}



void _d_read_cb(uv_stream_t *stream, ssize_t nread, const uv_buf_t *uv_buf) {
//    printf("[d_read_cb]: stream p: %p, nread: %ld\n", stream, nread);
    if(nread < 0) {
        printf("[d_raed_cb], nread(%ld) < 0\n", nread);
        return;
    }
    // printf("[d_read_cb]: [%.*s]\n", (int)uv_buf->len, uv_buf->base);
    dubbo_client *client = (dubbo_client*)stream->data;
    client->buf->write_idx += nread;
//    printf("[d_read_cb]: add write idx to %d\n", client->buf->write_idx);
    byte_buf_t *buf = client->buf;
    while(buf->read_idx < buf->write_idx) {
//        printf("[d_read_cb]: d_while ridx: %d, widx: %d, state: %d\n", buf->read_idx, buf->write_idx, client->read_state);
        if(client->read_state == 0) {
            if(buf->write_idx - buf->read_idx >= 4) {
                if ((buf->buf[buf->read_idx] & 0xff) == 0xda && (buf->buf[buf->read_idx + 1] & 0xff) == 0xbb) {
                    buf->read_idx += 4;
                    client->read_state = (client->read_state + 1);
//                    printf("[d_read_cb]: get header, status: %x %x\n", buf->buf[buf->read_idx - 4 + 2] & 0xff,
//                           buf->buf[buf->read_idx - 4 + 3] & 0xff);
                }
            } else {
                break;
            }
        }
        if(client->read_state == 1) {
            if (buf->write_idx - buf->read_idx >= 8) {
                client->dubbo_resp.id = bytes2ll(buf->buf + buf->read_idx);
                buf->read_idx += 8;
                client->read_state = (client->read_state + 1);
//                printf("[d_read_cb]get dubbo resp id: %lld\n", client->dubbo_resp.id);
            } else {
                break;
            }
        }
        if(client->read_state == 2) {
            if (buf->write_idx - buf->read_idx >= 4) {
                client->dubbo_resp.data_len = bytes2int(buf->buf + buf->read_idx);
                buf->read_idx += 4;
                client->read_state = (client->read_state + 1);
//                printf("[d_read_cb]: get dubbo data len: %d\n", client->dubbo_resp.data_len);
            } else {
                break;
            }
        }
        if(client->read_state == 3) {
//            printf("[d_read_cb]: need data_len: %d\n", client->dubbo_resp.data_len);
            if (buf->write_idx - buf->read_idx >= client->dubbo_resp.data_len) {
                client->dubbo_resp.result = (char *) malloc(client->dubbo_resp.data_len + FK);                       // TODO free
                strncpy(client->dubbo_resp.result, buf->buf + buf->read_idx, client->dubbo_resp.data_len);
//                printf("get dubbo result: [...]\n"); //, client->dubbo_resp.result); // %s 危险.
                buf->read_idx += client->dubbo_resp.data_len;
                client->read_state = 0;
                (*client->cbs[client->dubbo_resp.id])(&client->dubbo_resp,
                                                      (client->contexts[client->dubbo_resp.id]));             // result的内存自行free
                client->cbs.erase(client->cbs.find(client->dubbo_resp.id));
                client->contexts.erase(client->contexts.find(client->dubbo_resp.id));
            } else {
                break;
            }
        }
    }
//    printf("[d_read_cb]: b_leave, r: %d, w: %d\n", buf->read_idx, buf->write_idx);
    return;
}

void _d_on_connect(uv_connect_t *conn, int status) {
    if(status < 0) {
        fprintf(stderr, "new connect error %s, lets retry\n", uv_strerror(status));
    } else {
//        printf("[d_on_connect]: connect success, status: %d\n", status);
        dubbo_client* d_client = (dubbo_client*)conn->data;
        d_client->connected = 1;
        uv_read_start((uv_stream_t*)&d_client->socket, _d_alloc_cb, _d_read_cb);
        handle_queue(d_client);
    }
}

void dubbo_fetch(dubbo_client *client, dubbo_request *request, dubbo_callback cb, stream_context *context){
//    printf("[dubbo_fetch]: start fetch\n");
    client->queue.push(request);

    if(client->cbs.find(request->id) != client->cbs.end()){
        printf("id %lld already in cbs\n", request->id);
    }
    client->cbs[request->id] = cb;
    client->contexts[request->id] = context;
//    printf("add callback to map\n");
    handle_queue(client);
}

dubbo_client* create_dubbo_client(char *host, int port, uv_loop_t *io_loop) {
    // dubbo_client *client = (dubbo_client*)malloc(sizeof(dubbo_client));
    dubbo_client *client = new dubbo_client();
    client->conn.data = client;
    client->socket.data = client;
    client->buf_pool = default_buf_pool;
    client->buf = NULL;
    client->connected = 0;
    uv_tcp_init(io_loop, &client->socket);           // uv_tcp_init_ex to set flags
    struct sockaddr_in dest;
    uv_ip4_addr(host, port, &dest);
    uv_tcp_connect(&client->conn, &client->socket, (struct sockaddr*)&dest, _d_on_connect);
    return client;
}
/*

#include <time.h>
#include <sys/time.h>
#include <iostream>

struct timeval c0;
int cnt = 0;

void test_callback(dubbo_response *resp, struct dubbo_client *client) {
    struct timeval c1;
    gettimeofday(&c1, NULL);
    cnt++;
    printf("[test cb]: get resp id: %lld, data len: %d, data: [%.*s]\n", resp->id, resp->data_len, resp->data_len, resp->result);
    std::cout<<"cnt: "<<cnt<<"\t"<<c1.tv_sec-c0.tv_sec<<' '<<(c1.tv_usec-c0.tv_usec)<<'\n';
    free(resp->result);
}

int main() {
    dubbo_request dr[2048];
    uv_loop_t io_loop;
    uv_loop_init(&io_loop);
    dubbo_client *client = create_dubbo_client("127.0.0.1", 20880, &io_loop);
    gettimeofday(&c0, NULL);
    std::cout<<c0.tv_usec<<'\n';
    for(int i=0; i<2048; i++) {
        dr[i].interface_name = "com.alibaba.dubbo.performance.demo.provider.IHelloService";
        dr[i].method_name = "hash";
        dr[i].dubbo_version = "2.0.1";
        dr[i].version = "0.0.0";
        dr[i].two_way = 1;
        dr[i].args = "test";
        dr[i].parameter_types_string = "Ljava/lang/String;";
        dr[i].id = i;
        dubbo_fetch(client, &dr[i], &test_callback);
    }
    uv_run(&io_loop, UV_RUN_DEFAULT);
}
*/
