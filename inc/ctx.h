#pragma once

#include <cstring>
#include <string>
#include <unordered_map>

struct response_s
{
private:
    std::string http_version = "HTTP/1.1";                // 默认 HTTP 版本
    int status_code = 200;                                // 默认状态码
    std::string status_message = "OK";                    // 默认状态消息
    std::unordered_map<std::string, std::string> headers; // 响应头
    std::string body;                                     // 响应正文

    // 获取默认状态消息
    auto getDefaultStatusMessage(int code) -> std::string
    {
        switch (code)
        {
        case 100:
            return "Continue";
        case 201:
            return "Created";
        case 202:
            return "Accepted";
        case 204:
            return "No Content";
        case 206:
            return "Partial Content";
        case 301:
            return "Moved Permanently";
        case 302:
            return "Found";
        case 304:
            return "Not Modified";
        case 400:
            return "Bad Request";
        case 401:
            return "Unauthorized";
        case 403:
            return "Forbidden";
        case 404:
            return "Not Found";
        case 416:
            return "Range Not Satisfiable";
        case 418:
            return "I'm a teapot";
        case 500:
            return "Internal Server Error";
        case 501:
            return "Not Implemented";
        default:
            return "Unknown";
        }
    }

public:
    // 设置 HTTP 版本
    auto setVersion(const std::string &version) -> response_s *
    {
        http_version = version;
        return this;
    }

    // 设置状态码和状态消息
    auto setStatus(int code, const std::string &message = "") -> response_s *
    {
        status_code = code;
        status_message =
            message.empty() ? getDefaultStatusMessage(code) : message;
        return this;
    }

    // 添加响应头
    auto addHeader(const std::string &key,
                   const std::string &value) -> response_s *
    {
        headers[key] = value;
        return this;
    }

    // 设置响应正文
    auto setBody(const std::string &response_body) -> response_s *
    {
        body = response_body;
        addHeader("Content-Length",
                  std::to_string(body.size())); // 自动设置 Content-Length
        return this;
    }

    // 构建完整的响应字符串
    auto build(char *&buf) const -> int
    {
        // 1. 计算所需的总缓冲区大小
        size_t total_size = 0;

        // 起始行长度: "HTTP/1.1 200 OK\r\n"
        total_size += http_version.size() + 1 +
                      std::to_string(status_code).size() + 1 +
                      status_message.size() + 2;

        // 每个头部的长度: "Key: Value\r\n"
        for (const auto &[key, value] : headers)
        {
            total_size += key.size() + 2 + value.size() + 2;
        }

        // 空行（分隔头部和正文）
        total_size += 2;

        // 正文的长度
        total_size += body.size();

        // 2. 分配内存：+1 用于字符串结束符 '\0'
        buf = new char[total_size + 1];
        char *write_ptr = buf;

        // 3. 构建 HTTP 响应字符串

        // 写入起始行
        write_ptr +=
            std::sprintf(write_ptr, "%s %d %s\r\n", http_version.c_str(),
                         status_code, status_message.c_str());

        // 写入头部
        for (const auto &[key, value] : headers)
        {
            write_ptr += std::sprintf(write_ptr, "%s: %s\r\n", key.c_str(),
                                      value.c_str());
        }

        // 写入空行
        write_ptr += std::sprintf(write_ptr, "\r\n");

        // 写入正文
        if (!body.empty())
        {
            memcpy(write_ptr, body.c_str(), body.size());
            write_ptr += body.size();
        }

        // 添加终止符 '\0'
        *write_ptr = '\0';

        // 4. 返回实际字符串长度
        return static_cast<int>(total_size);
    }
};

struct request_s
{
    std::string method;
    std::string url;
    std::string version;
    std::unordered_map<std::string, std::string> headers;
    std::string body;
};