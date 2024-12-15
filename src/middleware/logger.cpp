#include "middleware/logger.h"
#include "router.h"
#include <chrono>
#include <functional>

void logger::operator()(Context *ctx)
{
    // 记录开始时间
    auto start = std::chrono::high_resolution_clock::now();

    ctx->next();

    // 记录结束时间
    auto end = std::chrono::high_resolution_clock::now();

    // 计算时差（以毫秒为单位）
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    auto now = std::chrono::system_clock::now();

    log(now, ctx->getRequest()->method, ctx->getRequest()->url, ctx->getResponse()->getStatus(), duration);
}