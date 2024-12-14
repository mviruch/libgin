#pragma once

#include "ctx.h"
#include <functional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

class node;
class Engine;
class Context;

using Params = std::unordered_map<std::string, std::string>;
using Handler = std::function<void(Context *)>;
using RouteHandler = std::function<void(request_s *, response_s *, Context *)>;
using HandlerChain = std::vector<Handler>;

struct Context
{
public:
    // Context(
    //     const std::string &method, const std::string &path, Params params,
    //     HandlerChain handlerChain) : method(method), path(path),
    //     params(params), handlerChain(handlerChain), index(-1) {}

    Context(request_s *req, response_s *res, Params params,
            HandlerChain handlerChain)
        : req(req), res(res), params(std::move(params)),
          handlerChain(std::move(handlerChain)), index(-1) {}

    void next();
    void abort();
    auto getParam(const std::string &key, std::string &param) -> bool;
    auto getRequest() -> request_s * { return req; }
    auto getResponse() -> response_s * { return res; }

private:
    request_s *req;
    response_s *res;
    Params params;
    HandlerChain handlerChain;
    size_t index = 0;
};

struct RouterGroup
{
public:
    auto group(std::string relativePath) -> RouterGroup *;
    void handle(const std::string &method, const std::string &path,
                RouteHandler handler);
    void use(Handler handler);

protected:
    std::unordered_map<std::string, node *> trees;
    // HandlerChain handlers;

    RouterGroup(std::string relativePath, HandlerChain handlers, Engine *engine)
        : basePath(std::move(relativePath)), handlers(std::move(handlers)),
          engine(engine) {}

private:
    std::string basePath;
    HandlerChain handlers;
    Engine *engine;

    auto calculateAbsolutePath(const std::string &relativePath) -> std::string;
    auto combineHandlers(Handler handler) -> HandlerChain;
};

struct Engine : public RouterGroup
{
public:
    Engine() : RouterGroup("", {}, this) {}
    ~Engine() = default;

    // void ServeHTTP(const std::string &method, const std::string &path);
    void ServeHTTP(request_s &req, response_s &res);
    void NoRoute(RouteHandler handler)
    {
        noroute = [handler](Context *ctx)
        {
            handler(ctx->getRequest(), ctx->getResponse(), ctx);
        };
    }

private:
    Handler noroute;
};
