# libgin

## Example

```c++
#include "ctx.h"
#include "gin.h"
#include "router.h"
#include <cstdio>
#include <fmt/core.h>
#include <iostream>

void setupRouter(Engine *engine);

auto main() -> int
{
    int err;
    uv_http_s http;
    Engine r;
    auto loop = uv_default_loop();

    fmt::println("Hello, World!");

    setupRouter(&r);

    err = uv_http_init(&http, loop, &r);
    if (err)
    {
        fmt::print(stderr, "uv_http_init error {}\n", uv_strerror(err));
        return err;
    }

    err = uv_http_listen(&http, "127.0.0.1", 3000);
    if (err)
    {
        fmt::print(stderr, "uv_http_listen error {}\n", uv_strerror(err));
        return err;
    }

    return uv_run(loop, UV_RUN_DEFAULT);
}

void setupRouter(Engine *r)
{
    auto v1 = r->group("/v1");
    v1->handle("GET", "/hello",
               [](request_s *req, response_s *res, Context *ctx)
               {
                   res->setStatus(200)
                       ->addHeader("Content-Type", "text/plain")
                       ->setBody("Hello, World!");
               });

    v1->handle("GET", "/hello/:name",
               [](request_s *req, response_s *res, Context *ctx)
               {
                   std::string name;
                   if (!ctx->getParam("name", name))
                   {
                       res->setStatus(400)
                           ->addHeader("Content-Type", "text/plain")
                           ->setBody("Bad Request");
                       return;
                   }
                   res->setStatus(200)
                       ->addHeader("Content-Type", "text/plain")
                       ->setBody(fmt::format("Hello, {}!", name));
               });
}
```

## Benchmark

```shell
$ ab -c 20 -n 200000 http://127.0.0.1:3000/v1/hello/mviruch
This is ApacheBench, Version 2.3 <$Revision: 1913912 $>
Copyright 1996 Adam Twiss, Zeus Technology Ltd, http://www.zeustech.net/
Licensed to The Apache Software Foundation, http://www.apache.org/

Benchmarking 127.0.0.1 (be patient)
Completed 20000 requests
Completed 40000 requests
Completed 60000 requests
Completed 80000 requests
Completed 100000 requests
Completed 120000 requests
Completed 140000 requests
Completed 160000 requests
Completed 180000 requests
Completed 200000 requests
Finished 200000 requests


Server Software:        
Server Hostname:        127.0.0.1
Server Port:            3000

Document Path:          /v1/hello/mviruch
Document Length:        15 bytes

Concurrency Level:      20
Time taken for tests:   6.618 seconds
Complete requests:      200000
Failed requests:        0
Total transferred:      17000000 bytes
HTML transferred:       3000000 bytes
Requests per second:    30219.19 [#/sec] (mean)
Time per request:       0.662 [ms] (mean)
Time per request:       0.033 [ms] (mean, across all concurrent requests)
Transfer rate:          2508.43 [Kbytes/sec] received

Connection Times (ms)
              min  mean[+/-sd] median   max
Connect:        0    0   0.1      0       3
Processing:     0    0   0.1      0       3
Waiting:        0    0   0.1      0       3
Total:          0    1   0.1      1       3

Percentage of the requests served within a certain time (ms)
  50%      1
  66%      1
  75%      1
  80%      1
  90%      1
  95%      1
  98%      1
  99%      1
 100%      3 (longest request)
```