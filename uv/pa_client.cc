#include "pa_client.h"
#include "utils.h"
#include "log.h"

#define FK 1

#define MIN(a,b) ((a)<(b)?(a):(b))
#define MAX(a,b) ((a)>(b)?(a):(b))

void _pa_alloc_cb(uv_handle_t *handle, size_t suggested_size, uv_buf_t *uv_buf) {
    unsigned int remain = 0;
    pa_client *client = (pa_client*)handle->data;
    if(client->buf) {
        remain = client->buf->size - client->buf->write_idx;
    }
    if(remain < suggested_size / 3) {
        byte_buf_t *new_buf = pool_malloc(&client->buf_pool, suggested_size);
        if(client->buf) {
            int ri = client->buf->read_idx;
            int wi = client->buf->write_idx;

            for(int i=ri; i<wi; i++) {
                new_buf->buf[i-ri] = client->buf->buf[i]; new_buf->write_idx++;
            }
            pool_free(&client->buf_pool, client->buf);
        }
        client->buf = new_buf;
    }
    uv_buf->base = client->buf->buf + client->buf->write_idx;
    uv_buf->len = client->buf->size - client->buf->write_idx;
}

void _pa_read_cb(uv_stream_t *stream, ssize_t nread, const uv_buf_t *uv_buf) {
    if(nread < 0) {
        ERROR("nread %ld < 0", nread);
        return;
    }
    (void)uv_buf;
    pa_client *client = (pa_client*)stream->data;
    client->buf->write_idx += nread;
    byte_buf_t *buf = client->buf;
    while(buf->read_idx < buf->write_idx) {
        if(client->read_state == 0) {
            if(buf->write_idx - buf->read_idx >= 4) {
                client->a_resp.id = bytes2int(buf->buf + buf->read_idx);
                buf->read_idx += 4;
                client->read_state++;
            } else {
                break;
            }
        }
        if(client->read_state == 1) {
            if(buf->write_idx - buf->read_idx >= 4) {
                client->a_resp.data_len = bytes2int(buf->buf + buf->read_idx);
                buf->read_idx += 4;
                client->read_state++;
            } else {
                break;
            }
        }
        if(client->read_state == 2) {
            if(buf->write_idx - buf->read_idx >= client->a_resp.data_len) {
                client->a_resp.result = (char*)malloc(client->a_resp.data_len + FK);
                strncpy(client->a_resp.result, buf->buf + buf->read_idx, client->a_resp.data_len);
                client->read_state = 0;
                buf->read_idx += client->a_resp.data_len;
                (*client->callbacks[client->a_resp.id])(&client->a_resp, client->contexts[client->a_resp.id]);  // 考虑一下有没有内存问题，注意自行 free result
//                INFO("[pa_read] get act resp [%d] [%d]", client->a_resp.id, client->a_resp.data_len);
                client->callbacks.erase(client->callbacks.find(client->a_resp.id));
                client->contexts.erase(client->contexts.find(client->a_resp.id));
            } else {
                break;
            }
        }
    }
    return;
}

uv_buf_t _pa_act_request_encode(act_request *request) {
    // TODO 可以用指针优化内存
    int total_len = 4 + 4 + request->p_len;
    uv_buf_t res;
    char *buffer = (char*)malloc(total_len + FK);
    write_int(buffer, request->id);
    write_int(buffer+4, request->p_len);
    strncpy(buffer+8, request->parameter, request->p_len);
    res.base = buffer;
    res.len = total_len;
    return res;
}

struct _pa_write_context {
    uv_buf_t *bufs;
    int bcnt;
};

void _pa_write_cb(uv_write_t *req, int status) {
    if(status) {
        ERROR("write cb error %s", uv_strerror(status));
        return;
    }
    _pa_write_context *ctx = (_pa_write_context*)req->data;
    for(int i=0; i<ctx->bcnt; i++) {
        free(ctx->bufs[i].base);
    }
    free(ctx->bufs); free(ctx);
    free(req);
}

void _free_act_request(act_request *request) {
    free(request->parameter);
    free(request);
}

void _pa_handle_queue(pa_client *client) {
//    INFO("in handle queue");
    if(!client->connected) {
        INFO("still not connected");
        return;
    }
    uv_buf_t *bufs = (uv_buf_t*)malloc(sizeof(uv_buf_t) * client->que.size() + FK);
    int bcnt = 0;
    while(!client->que.empty()) {
        act_request *request = client->que.front(); client->que.pop();
        bufs[bcnt++] = _pa_act_request_encode(request);
        _free_act_request(request);
    }
    if(bcnt <= 0) {
        free(bufs);
        return;
    }
    uv_write_t *w_req = (uv_write_t*)malloc(sizeof(uv_write_t));
    _pa_write_context *ctx = (_pa_write_context*)malloc(sizeof(_pa_write_context));
    ctx->bcnt = bcnt; ctx->bufs = bufs;
    w_req->data = ctx;
    int ret = uv_write(w_req, (uv_stream_t*) &client->stream, bufs, bcnt, _pa_write_cb);
    if(ret) {
        ERROR("pa client write error : %s-%s", uv_err_name(ret), uv_strerror(ret));
    }
}

uv_buf_t* _pa_encode_act_key(act_reuse_key *key) {
    // TODO: 处理 url decode
    int total_len = 4 + key->interface_len + 4 + key->method_len + 4 + key->pts_len + 2;
    char *buffer = (char*)malloc(total_len + FK);
    buffer[0] = 0xab; buffer[1] = 0xcd;
    int pos = 2;
    write_int(buffer+pos, key->interface_len);  pos += 4;
    strncpy(buffer+pos, key->interface, key->interface_len); pos += key->interface_len;
    write_int(buffer+pos, key->method_len); pos += 4;
    strncpy(buffer+pos, key->method, key->method_len);      pos += key->method_len;
    write_int(buffer+pos, key->pts_len);    pos += 4;
    strncpy(buffer+pos, key->pts, key->pts_len);
    uv_buf_t *res = (uv_buf_t*)malloc(sizeof(uv_buf_t));
    res->base = buffer;
    res->len = total_len;
    return res;
}

struct key_write_context {
    pa_client *client;
    uv_buf_t *buf;
};

void _pa_key_write_cb(uv_write_t *req, int status) {
//    INFO("[pa_key_write_cb]");
    if(status) {
        ERROR("pa key write_cb error %s", uv_strerror(status));
        return;
    }
    key_write_context *ctx = (key_write_context*)req->data;
    pa_client *client = ctx->client;    client->connected = 1;
    INFO("write act header finish, set connected!!");
    _pa_handle_queue(client);
    INFO("free 4 p: %p %p %p %p", ctx->buf->base, ctx->buf, ctx, req);
    free(ctx->buf->base);
    free(ctx->buf);
    free(ctx);
    free(req);
}

void _pa_on_conn(uv_connect_t *conn, int status) {
    INFO("[pa_on_conn]");
    if(status < 0) {
        ERROR("new conn error: %s", uv_strerror(status));
    } else {
        pa_client *client = (pa_client*)conn->data;
        uv_buf_t *buf = _pa_encode_act_key(client->key);
        uv_write_t *w_req = (uv_write_t*)malloc(sizeof(uv_write_t));
        key_write_context *ctx = (key_write_context*)malloc(sizeof(key_write_context));
        ctx->client = client;   ctx->buf = buf;
        w_req->data = ctx;
        INFO("[pa_on_conn]: uv_write to pa");
        int ret = uv_write(w_req, (uv_stream_t*)&client->stream, buf, 1, _pa_key_write_cb);
        if (ret) {
            ERROR("pa key write error: %s", uv_strerror(ret));
        }
//        client->connected = 1;
        uv_read_start((uv_stream_t*)&client->stream, _pa_alloc_cb, _pa_read_cb);
        _pa_handle_queue(client);
    }
}

pa_client* create_pa_client(char *host, int port, act_reuse_key *key, uv_loop_t *io_loop) {
    pa_client *client = new pa_client();
    client->conn.data = client;
    client->key = key;
    client->stream.data = client;
    client->buf_pool = default_buf_pool;
    client->buf = NULL;
    client->connected = 0;
    uv_tcp_init(io_loop, &client->stream);
    int fd;
    int opt_v = 1;
    uv_fileno((uv_handle_t*)&client->stream, &fd);
#ifdef __linux__
    INFO("set pa_client socket TCP_CORK");
    setsockopt(fd, IPPROTO_TCP, TCP_CORK, &opt_v, sizeof(opt_v));
#elif defined(__APPLE__)
    setsockopt(fd, IPPROTO_TCP, TCP_NOPUSH, &opt_v, sizeof(opt_v));
#endif
    uv_tcp_keepalive(&client->stream, 1, 120);
//    uv_tcp_nodelay(&client->stream, 1);
    struct sockaddr_in dest;
    uv_ip4_addr(host, port, &dest);
    uv_tcp_connect(&client->conn, &client->stream, (struct sockaddr*)&dest, _pa_on_conn);
    return client;
}

void pa_client_fetch(pa_client *client, act_request *req, pa_callback cb, h_context *context) {
//    INFO("[pa_client_fetch]: get new fetch");
    client->que.push(req);
    client->callbacks[req->id] = cb;
    client->contexts[req->id] = context;
    _pa_handle_queue(client);
}


