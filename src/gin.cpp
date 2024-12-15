#include "gin.h"
#include "reader.h"
#include "router.h"
#include <cstring>
#include <iostream>
#include <streambuf>
#include <string>
#include <unordered_map>

#if !defined(container_of)
#if defined(__GNUC__) || defined(__clang__)
#define container_of(ptr, type, member)                    \
    ({                                                     \
        const typeof(((type *)0)->member) *__mptr = (ptr); \
        (type *)((char *)__mptr - offsetof(type, member)); \
    })
#else
#define container_of(ptr, type, member) \
    ((type *)((char *)(ptr) - offsetof(type, member)))
#endif
#endif

void onconnection(uv_stream_t *server, int status);
void onalloc(uv_handle_t *handle, size_t suggested_size, uv_buf_t *buf);
void onread(uv_stream_t *stream, ssize_t nread, const uv_buf_t *buf);

auto onmessagebegin(llhttp_t *parser) -> int;
auto onurl(llhttp_t *parser, const char *at, size_t length) -> int;
auto onstatus(llhttp_t *parser, const char *at, size_t length) -> int;
auto onmethod(llhttp_t *parser, const char *at, size_t length) -> int;
auto onversion(llhttp_t *parser, const char *at, size_t length) -> int;
auto onheaderfield(llhttp_t *parser, const char *at, size_t length) -> int;
auto onheadervalue(llhttp_t *parser, const char *at, size_t length) -> int;
auto onheaderscomplete(llhttp_t *parser) -> int;
auto onbody(llhttp_t *parser, const char *at, size_t length) -> int;
auto onmessagecomplete(llhttp_t *parser) -> int;
void defaulthttpcb(uv_http_conn_s *conn, uv_http_event_t event, void *data);
void httpcb(uv_http_conn_s *conn, uv_http_event_t event, void *data);
void write_cb(uv_write_t *req, int status);
void request_close(uv_http_conn_s *conn);
// void request_done_async(uv_async_t *handle);

auto uv_http_init(uv_http_s *http, uv_loop_s *loop, Engine *engine) -> int
{
    http->loop = loop;
    // http->cb = cb;
    http->engine = engine;
    int err = uv_tcp_init(loop, &http->server);
    return err;
}

auto uv_http_listen(uv_http_s *http, const char *ip, int port) -> int
{
    struct sockaddr_in addr;
    int err = uv_ip4_addr(ip, port, &addr);
    if (err)
    {
        return err;
    }

    err = uv_tcp_bind(&http->server, (const struct sockaddr *)&addr, 0);
    if (err)
    {
        return err;
    }

    err = uv_listen((uv_stream_t *)&http->server, SOMAXCONN, onconnection);
    return err;
}

auto uv_http_conn_init(uv_http_conn_s *conn, uv_http_s *http) -> int
{
    llhttp_settings_init(&conn->settings);
    conn->settings.on_message_begin = onmessagebegin;
    conn->settings.on_url = onurl;
    conn->settings.on_status = onstatus;
    conn->settings.on_method = onmethod;
    conn->settings.on_version = onversion;
    conn->settings.on_header_field = onheaderfield;
    conn->settings.on_header_value = onheadervalue;
    conn->settings.on_headers_complete = onheaderscomplete;
    conn->settings.on_body = onbody;
    conn->settings.on_message_complete = onmessagecomplete;
    llhttp_init(&conn->parser, HTTP_REQUEST, &conn->settings);

    conn->buf = new ThreadSafeReaderStreambuf(1024);
    conn->request.body = new std::istream(conn->buf);
    conn->http = http;

    // int err = uv_async_init(http->loop, &conn->async, request_done_async);

    return uv_tcp_init(http->loop, &conn->client);
}

void request_done(uv_work_t *req, int status)
{
    uv_http_conn_s *conn = container_of(req, uv_http_conn_s, work);

    if (conn->closed)
    {
        request_close(conn);
        return;
    }

    int total_size = 0;
    char *buf;
    total_size = conn->response.build(buf);
    conn->response_str = buf;

    uv_buf_t resbuf = uv_buf_init(buf, total_size);
    auto *req_write = new uv_write_t();
    req_write->data = conn;

    uv_write(req_write, (uv_stream_t *)&conn->client, &resbuf, 1, write_cb);
}

void onconnection(uv_stream_t *server, int status)
{
    if (status < 0)
    {
        return;
    }

    uv_http_s *http = container_of((uv_tcp_t *)server, uv_http_s, server);

    auto *client = new uv_http_conn_s(); //(uv_http_conn_s *)calloc(1,
                                         // sizeof(uv_http_conn_s));
    uv_http_conn_init(client, http);

    if (uv_accept(server, (uv_stream_t *)&client->client) == 0)
    {
        uv_read_start((uv_stream_t *)client, onalloc, onread);
    }
    else
    {
        uv_close((uv_handle_t *)client, nullptr);
    }
}

void onalloc(uv_handle_t *handle, size_t suggested_size, uv_buf_t *buf)
{
    buf->base = (char *)malloc(suggested_size);
    buf->len = suggested_size;
}

void onread(uv_stream_t *stream, ssize_t nread, const uv_buf_t *buf)
{
    int ret;
    uv_http_conn_s *client =
        container_of((uv_tcp_t *)stream, uv_http_conn_s, client);

    if (nread > 0)
    {
        ret = llhttp_execute(&client->parser, buf->base, nread);
        if (ret != HPE_OK)
        {
            httpcb(client, UV_HTTP_ERROR,
                   (void *)llhttp_errno_name((llhttp_errno_t)ret));
            request_close(client);
        }
    }
    else if (nread < 0)
    {
        httpcb(client, UV_HTTP_CLOSE, (void *)uv_strerror(nread));
        request_close(client);
    }

    free(buf->base);
}

auto onmessagebegin(llhttp_t *parser) -> int
{
    uv_http_conn_s *conn = container_of(parser, uv_http_conn_s, parser);
    return 0;
}

auto onurl(llhttp_t *parser, const char *at, size_t length) -> int
{
    uv_http_conn_s *conn = container_of(parser, uv_http_conn_s, parser);
    conn->request.url = std::string(at, length);
    return 0;
}

auto onstatus(llhttp_t *parser, const char *at, size_t length) -> int
{
    uv_http_conn_s *conn = container_of(parser, uv_http_conn_s, parser);
    return 0;
}

auto onmethod(llhttp_t *parser, const char *at, size_t length) -> int
{
    uv_http_conn_s *conn = container_of(parser, uv_http_conn_s, parser);
    auto x = std::string(at, length);
    conn->request.method = x;
    return 0;
}

auto onversion(llhttp_t *parser, const char *at, size_t length) -> int
{
    uv_http_conn_s *conn = container_of(parser, uv_http_conn_s, parser);
    conn->request.version = std::string(at, length);
    return 0;
}

auto onheaderfield(llhttp_t *parser, const char *at, size_t length) -> int
{
    uv_http_conn_s *conn = container_of(parser, uv_http_conn_s, parser);
    conn->currentheaderfield = std::string(at, length);
    return 0;
}

auto onheadervalue(llhttp_t *parser, const char *at, size_t length) -> int
{
    uv_http_conn_s *conn = container_of(parser, uv_http_conn_s, parser);
    conn->request.headers[conn->currentheaderfield] = std::string(at, length);
    return 0;
}

auto onheaderscomplete(llhttp_t *parser) -> int
{
    uv_thread_t tid;
    uv_http_conn_s *conn = container_of(parser, uv_http_conn_s, parser);

    auto contentlength = conn->request.headers.find("Content-Length");
    if (contentlength != conn->request.headers.end())
    {
        auto *buf = static_cast<ThreadSafeReaderStreambuf *>(conn->buf);
        buf->setRemainingSize(std::stoi(contentlength->second));
    }

    uv_queue_work(
        conn->http->loop, &conn->work,
        [](uv_work_t *req)
        {
            uv_http_conn_s *conn = container_of(req, uv_http_conn_s, work);
            httpcb(conn, UV_HTTP_MESSAGE, &conn->request);
        },
        request_done);

    return 0;
}

auto onbody(llhttp_t *parser, const char *at, size_t length) -> int
{
    uv_http_conn_s *conn = container_of(parser, uv_http_conn_s, parser);
    auto *buf = static_cast<ThreadSafeReaderStreambuf *>(conn->buf);
    // fmt::println("Body length: {}", length);
    buf->write(at, length);
    return 0;
}

auto onmessagecomplete(llhttp_t *parser) -> int
{
    uv_http_conn_s *conn = container_of(parser, uv_http_conn_s, parser);
    return 0;
}

void defaulthttpcb(uv_http_conn_s *conn, uv_http_event_t event, void *data)
{
    switch (event)
    {
    case UV_HTTP_ERROR:
    {
        const char *err = (const char *)data;
        break;
    }
    case UV_HTTP_CLOSE:
    {
        const char *err = (const char *)data;
        break;
    }
    case UV_HTTP_MESSAGE:
    {
        conn->http->engine->ServeHTTP(conn->request, conn->response);
        break;
    }
    default:
        // Handle other events
        break;
    }
}

void httpcb(uv_http_conn_s *conn, uv_http_event_t event, void *data)
{
    auto http = conn->http;
    // uv_http_cb cb = http->cb ? http->cb : defaulthttpcb;
    defaulthttpcb(conn, event, data);
}

void request_close(uv_http_conn_s *conn)
{
    uv_close((uv_handle_t *)&conn->client, [](uv_handle_t *handle)
             {
        uv_http_conn_s *conn =
            container_of((uv_tcp_t *)handle, uv_http_conn_s, client);
        delete static_cast<ThreadSafeReaderStreambuf *>(conn->buf);
        delete conn->response_str;
        delete conn->request.body;
        delete conn; });
}

void write_cb(uv_write_t *req, int status)
{
    uv_stream_t *stream = req->handle;
    uv_http_conn_s *conn =
        container_of((uv_tcp_t *)stream, uv_http_conn_s, client);

    delete req;
    request_close(conn);
}
