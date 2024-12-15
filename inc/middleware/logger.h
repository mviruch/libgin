#pragma once

#include <chrono>
#include <string>

struct Context;

typedef void (*logfunc)(std::chrono::_V2::system_clock::time_point now, std::string method, std::string uri, int status, std::chrono::microseconds duration);

struct logger
{
    logfunc log;
    void operator()(Context *ctx);
};