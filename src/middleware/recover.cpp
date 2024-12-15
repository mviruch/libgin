#include "middleware/recover.h"
#include "router.h"

void updateResponse(response_s *res, int status, const std::string &body)
{
    res->setStatus(status);
    res->setBody(body);
}

void recover::operator()(Context *ctx)
{
    try
    {
        ctx->next();
    }
    catch (const std::exception &e)
    {
        ctx->abort();
        updateResponse(ctx->getResponse(), 500, e.what());
    }
    catch (...)
    {
        ctx->abort();
        updateResponse(ctx->getResponse(), 500, "Internal Server Error");
    }
}