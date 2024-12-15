#pragma once

#include "llhttp.h"
#include "router.h"
#include <streambuf>
#include <uv.h>

struct uv_http_conn_s;
using request_t = struct request_s;

using uv_http_event_t = enum uv_http_event {
    UV_HTTP_ERROR,   /**< (const char*) Error. */
    UV_HTTP_CONNECT, /**< Connection establish. */
    UV_HTTP_ACCEPT,  /**< Connection accept. */
    UV_HTTP_CLOSE,   /**< Connection closed. */
    UV_HTTP_MESSAGE, /**< (#uv_http_message_t) HTTP request/response */
};

using uv_http_cb = void (*)(uv_http_conn_s *, uv_http_event_t, void *);

struct waitgroup_s
{
    int count;
    uv_mutex_t mutex;
    uv_cond_t cond;

    waitgroup_s()
    {
        count = 0;
        uv_mutex_init(&mutex);
        uv_cond_init(&cond);
    }

    ~waitgroup_s()
    {
        uv_mutex_destroy(&mutex);
        uv_cond_destroy(&cond);
    }

    void add(int delta)
    {
        uv_mutex_lock(&mutex);
        count += delta;
        uv_mutex_unlock(&mutex);
    }

    void done()
    {
        add(-1);
        uv_cond_signal(&cond);
    }

    void wait()
    {
        uv_mutex_lock(&mutex);
        while (count > 0)
        {
            uv_cond_wait(&cond, &mutex);
        }
        uv_mutex_unlock(&mutex);
    }
};

struct uv_http_s
{
    uv_loop_s *loop;
    uv_tcp_s server;
    // uv_http_cb cb;
    Engine *engine;
    // uv_http_data_t data;
    waitgroup_s wg;
};

struct uv_http_conn_s
{
    uv_tcp_s client;
    uv_http_s *http;
    uv_work_s work;

    // http 解析器
    llhttp_t parser;
    llhttp_settings_t settings;

    std::streambuf *buf;

    std::string currentheaderfield;

    request_t request;
    response_s response;
    char *response_str;

    bool closed = false;
};

auto uv_http_init(uv_http_s *http, uv_loop_s *loop, Engine *engine) -> int;
auto uv_http_listen(uv_http_s *http, const char *ip, int port) -> int;

auto uv_http_conn_init(uv_http_conn_s *conn, uv_http_s *http) -> int;

// int request_done(request_t *conn);
