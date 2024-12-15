#pragma once

struct Context;

// void recover(Context *ctx);

struct recover
{
    void operator()(Context *ctx);
};