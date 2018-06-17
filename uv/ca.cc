#include <stdio.h>
#include <stdlib.h>
#include <map>
#include <uv.h>
#include <unistd.h>

#include "bytebuf.h"
#include "utils.h"
#include "pa_client.h"
#include "http-parser/http_parser.h"
#include "ca.h"
#include "log.h"
#include "act.h"

#define FK 1

uv_loop_t io_loop;
ca_server ca_s;


h_context *create_h_context(ca_server *server, uv_tcp_t *channel) {
    // h_context *ctx = (h_context*)malloc(sizeof(h_context));
    h_context *ctx = new h_context();
//    INFO("new h_context p: %p", ctx);
    ctx->server = server;
    ctx->stream = channel;
    ctx->parser = (http_parser*)malloc(sizeof(http_parser));
//    INFO("parser: %p", ctx->parser);
    ctx->parser->data = ctx;
    ctx->next_new_req = 1;
    ctx->buf = NULL;
    ctx->buf_pool = default_buf_pool;
//    INFO("fine, will return ctx");
    return ctx;
}

void _ca_alloc_cb(uv_handle_t *handle, size_t suggested_size, uv_buf_t *uv_buf) {
    int remain = 0;
    h_context *ctx = (h_context*)(handle->data);
    ca_server *server = ctx->server;
    if(ctx->buf) {
        remain = ctx->buf->size - ctx->buf->write_idx;
    }
    if(remain < suggested_size/3) {
        // WARNING("create new buf");
        byte_buf_t *new_buf = pool_malloc(&server->buf_pool, suggested_size);
        if(ctx->buf) {
            int ri = ctx->buf->read_idx;
            int wi = ctx->buf->write_idx;
            for(int i=ri; i<wi; i++) {
                new_buf->buf[i-ri] = ctx->buf->buf[i];
                new_buf->write_idx++;
            }
            pool_free(&server->buf_pool, ctx->buf);
        }
        ctx->buf = new_buf;
    }
    uv_buf->base = ctx->buf->buf + ctx->buf->write_idx;
    uv_buf->len = ctx->buf->size - ctx->buf->write_idx;
}

void _ca_on_read(uv_stream_t *stream, ssize_t nread, const uv_buf_t *uv_buf) {
    if (nread < 0) {
        if(nread != UV_EOF) {
            ERROR("ca read err %s", uv_err_name(nread));
        }
        uv_close((uv_handle_t*)stream, NULL);
        return;
    }
    h_context *ctx = (h_context*)stream->data;
    ca_server *server = ctx->server;
    ctx->buf->write_idx += nread;
    byte_buf_t *buf = ctx->buf;
    //INFO("nread: %ld, %.*s", nread, buf->write_idx - buf->read_idx, buf->buf + buf->read_idx);
    if (ctx->next_new_req) {
//        INFO("new req, reinit parser");
        http_parser_init(ctx->parser, HTTP_REQUEST);
        ctx->next_new_req = 0;
    }
    // 暂时假设每次 read 只需要 execute 一次，因为一个tcp连接上应该只有一个http连接活跃
    size_t parsed = http_parser_execute(ctx->parser, &server->settings, buf->buf + buf->read_idx, buf->write_idx - buf->read_idx);
    buf->read_idx += parsed;
//    INFO("nread: %ld, nparsed: %lu, ri: %d, wi: %d", nread, parsed, buf->read_idx, buf->write_idx);
    // INFO("%lu bytes parsed", parsed);
}

uint bkdr_hash(char *s, int len, unsigned int init) {
    uint seed = 131;
    for(int i=0; i<len; i++) {
        init = init * seed + s[i];
    }
    return init;
}

void _ca_on_conn(uv_stream_t *stream, int status) {
    INFO("[_ca_on_conn]");
    if( status < 0) {
        ERROR("ca new conn err: %s", uv_strerror(status));
    }
    INFO("welcome socket get new conn!");
    uv_tcp_t *client = (uv_tcp_t*)malloc(sizeof(uv_tcp_t));
    ca_server *server = (ca_server*)stream->data;
    INFO("malloc new uv_tcp_t %p, server p: %p, loop p: %p", client, server, server->io_loop);
    uv_tcp_init(server->io_loop, client);
    h_context *ctx = create_h_context(server, client);
    INFO("get ctx: %p", ctx);
    client->data = ctx;
    INFO("malloc new h context %p", client->data);
    if(uv_accept(stream, (uv_stream_t*) client) == 0) {
        INFO("create new conn");
        uv_read_start((uv_stream_t*)client, _ca_alloc_cb, _ca_on_read);
    }
    else {
        ERROR("accept fail");
        uv_close((uv_handle_t*)client, NULL);
    }
}

int ca_server_listen(ca_server *server, char *host, int port) {
    uv_tcp_init_ex(server->io_loop, &server->server, AF_INET);
    int fd;
    int opt_v = 1;
    uv_fileno((uv_handle_t*)&server->server, &fd);
    setsockopt(fd, SOL_SOCKET, SO_REUSEPORT, &opt_v, sizeof(opt_v));
    uv_tcp_keepalive(&server->server, 1, 180);                               // enable ca server tcp keepalive
    struct sockaddr_in addr;
    uv_ip4_addr(host, port, &addr);
    uv_tcp_bind(&server->server, (const struct sockaddr*)&addr, 0);
    int r = uv_listen((uv_stream_t*)&server->server, 1024, _ca_on_conn);
    return r;
}

uint act_key_hash(ca_server *server, char *interface, int interface_len, char *method, int method_len, char *pts, int pts_len) {
    uint res = 0;
    res = (*(server->ca_hash))(interface, interface_len, res);
    res = (*(server->ca_hash))(method, method_len, res);
    res = (*(server->ca_hash))(pts, pts_len, res);
    return res;
}

int hex2int(char x) {
    int hi;
    if('0' <= x && x <= '9')
        hi = x - '0';
    else if ('a' <= x && x <= 'f')
        hi = x - 'a' + 10;
    else if ('A' <= x && x <= 'F')
        hi = x - 'A' + 10;
    return hi;
}

int strn_urlcpy(char *dest, char *src, int n) {
    int i = 0;
    int j = 0;
    while(i < n) {
        if(src[i] == '%' && i+2 < n) {
            dest[j++] = hex2int(src[i+1]) * 16 + hex2int(src[i+2]);
            i += 2;
        } else {
            dest[j++] = src[i];
        }
        i++;
    }
    return j;
}

void _act_done_write_cb(uv_write_t *req, int status) {
    if(status) {
        ERROR("act done w cb %s", uv_strerror(status));
    }
    uv_buf_t *buf = (uv_buf_t*)req->data;
//    INFO("free 3p: %p %p %p", buf->base, buf, req);
    free(buf->base);
    free(buf);
    free(req);
}

void act_done_callback(act_response *act_resp, h_context *ctx) {
    // INFO("act resp id: %d, resp: %.*s", act_resp->id, act_resp->data_len, act_resp->result);
//    INFO("[act_done_cb], id: %d, len: %d", act_resp->id, act_resp->data_len);
    int as, ae, i; as = ae = i= 0;
    while(i < act_resp->data_len) {
        if(act_resp->result[i] == '\n'){
            if(as == 0)
                as = i+1;
            else if (ae == 0)
                ae = i;
        }
        i++;
    }
    int content_len = ae - as;
//     INFO("ae: %d, as: %d, cl: %d", ae, as, content_len);
    if(content_len <= 0) {
//        ERROR("content_length %d !!!, ae: %d, as: %d, %.*s", content_len, ae, as, act_resp->data_len, act_resp->result);
        uv_write_t *w_req = (uv_write_t *) malloc(sizeof(uv_write_t));
        uv_buf_t *buf = (uv_buf_t *) malloc(sizeof(uv_buf_t));
        w_req->data = buf;
        buf->base = (char *) malloc(256 + FK);
        strncpy(buf->base, "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nContent-Length: ", 59);
        unsigned int ret = sprintf(buf->base + 59, "%d\r\n\r\n%d", content_len, 0);
//        INFO("buf: %p, base: %p, len: %lu, ret: %u", buf, buf->base, buf->len, ret);
        buf->len = 59 + ret;
        uv_write(w_req, (uv_stream_t *) ctx->stream, buf, 1, _act_done_write_cb);
    } else {
        uv_write_t *w_req = (uv_write_t *) malloc(sizeof(uv_write_t));
        uv_buf_t *buf = (uv_buf_t *) malloc(sizeof(uv_buf_t));
        w_req->data = buf;
        buf->base = (char *) malloc(256 + content_len + FK);
        strncpy(buf->base, "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nContent-Length: ", 59);
        unsigned int ret = sprintf(buf->base + 59, "%d\r\n\r\n%.*s", content_len, ae - as, act_resp->result + as);
//        INFO("buf: %p, base: %p, len: %lu, ret: %u", buf, buf->base, buf->len, ret);
        buf->len = 59 + ret;
        uv_write(w_req, (uv_stream_t *) ctx->stream, buf, 1, _act_done_write_cb);
    }
}

int _h_on_body(http_parser *parser, const char *data, size_t length) {
    // TODO handle body and send act request
    // INFO("on body, length: %lu, %.*s", length, length, data);
//    INFO("on body, length: %lu", length);
    h_context *ctx = (h_context*)parser->data;
    ca_server *server = ctx->server;
    int interface_v_start, method_v_start, pts_v_start, parameter_v_start;
    uint interface_v_hash, method_v_hash, pts_v_hash;
    int interface_v_end, method_v_end, pts_v_end, parameter_v_end;
    int _read_v=0;  // 0: interface, 1: method, 2: pts, 3: para
    int pos = 0; int i=0;
    char *p = (char*)data;
    while(i < length) {
        // INFO("i: %d, len: %d", i, length);
        while(i<length && data[i] != '=')   i++;
        uint hs = (*(server->ca_hash))(p+pos, i-pos, 0);
        if(hs == server->_interface_hash)
            _read_v = 0, interface_v_start=i+1;
        else if (hs == server->_method_hash)
            _read_v = 1, method_v_start=i+1;
        else if (hs == server->_pts_hash)
            _read_v = 2, pts_v_start=i+1;
        else
            _read_v = 3, parameter_v_start=i+1;
        pos = i+1;
        while(i<length && data[i] != '&')   i++;
        if(_read_v == 0)
            interface_v_end = i;
        else if (_read_v == 1)
            method_v_end = i;
        else if (_read_v == 2)
            pts_v_end = i;
        else
            parameter_v_end = i;
        pos = i+1;
    }
    int eidx = server->lb.nextIdx();
    char *host = server->lb.ends[eidx].host; int port = server->lb.ends[eidx].port;
    // INFO("will choose[%d] %s: %d", eidx, host, port);
    uint key_hs = act_key_hash(server, p+interface_v_start, interface_v_end-interface_v_start, p+method_v_start, method_v_end-method_v_start, p+pts_v_start, pts_v_end-pts_v_start);
    if(server->pa_client_maps[eidx].find(key_hs) == server->pa_client_maps[eidx].end()) {
        INFO("create new pa client");
        act_reuse_key *reuse_key = (act_reuse_key*)malloc(sizeof(act_reuse_key) + FK);
        reuse_key->interface = (char*)malloc(interface_v_end-interface_v_start + FK);
        reuse_key->method = (char*)malloc(method_v_end-method_v_start + FK);
        reuse_key->pts = (char*)malloc(pts_v_end-pts_v_start + FK);
        reuse_key->interface_len = strn_urlcpy(reuse_key->interface, p+interface_v_start, interface_v_end - interface_v_start);
        reuse_key->method_len = strn_urlcpy(reuse_key->method, p+method_v_start, method_v_end - method_v_start);
        reuse_key->pts_len = strn_urlcpy(reuse_key->pts, p+pts_v_start, pts_v_end-pts_v_start);
        pa_client *client = create_pa_client(host, port, reuse_key, server->io_loop);
        server->pa_client_maps[eidx][key_hs] = client;
    }
    act_request *req = (act_request*)malloc(sizeof(act_request) + FK);
    req->id = rand(); req->p_len = parameter_v_end - parameter_v_start;
    req->parameter = (char*)malloc(req->p_len + FK);
    strncpy(req->parameter, p+parameter_v_start, req->p_len);
//    INFO("send new act req [%d] [%d]", req->id, req->p_len);
//    ctx->buf->read_idx += length;
    pa_client_fetch(server->pa_client_maps[eidx][key_hs], req, &act_done_callback, ctx);
    return 0;
}

int _h_on_message_complete(http_parser *parser) {
//    INFO("finish one http request");
    h_context *ctx = (h_context*)parser->data;
    ctx->next_new_req = 1;
    return 0;
}

void ca_server_init(ca_server *server, uv_loop_t *loop, str_hash_func func) {
    server->ca_hash = func;
    server->_interface_hash = (*func)("interface", strlen("interface"), 0);
    server->_method_hash = (*func)("method", strlen("method"), 0);
    server->_pts_hash = (*func)("parameterTypesString", strlen("parameterTypesString"), 0);
    server->buf_pool = default_buf_pool;
    server->io_loop = loop;
    server->settings.on_body = &_h_on_body;
    server->settings.on_message_complete = &_h_on_message_complete;
    server->server.data = server;
}


int main(int argc, char *argv[]) {
#ifndef NDEBUG
    DEBUG_ENABLED = 1;
    DEBUG_TIMESTAMP = 1;
    DEBUG_PROGRESS = 1;
    DEBUG_PID = 1;
#endif
    uv_loop_init(&io_loop);
    srand((time(0) << 4) | getpid());
    ca_server_init(&ca_s, &io_loop, bkdr_hash);

    INFO("argc: %d", argc);
    int eds = argc/3;
    for(int i=0; i<eds; i++) {
        ca_s.lb.insert(argv[3*i], atoi(argv[3*i+1]), atoi(argv[3*i+2]));
        INFO("insert to lb: %s %s %s", argv[3*i], argv[3*i+1], argv[3*i+2]);
    }
    ca_s.lb.init_rate();
    ca_server_listen(&ca_s, "0.0.0.0", 20000);
    INFO("run server");
    uv_run(&io_loop, UV_RUN_DEFAULT);
}

