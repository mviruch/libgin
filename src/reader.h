#pragma once

#include <streambuf>
#include <vector>
#include <stdexcept>
#include <mutex>
#include <condition_variable>

class ReaderStreambuf : public std::streambuf
{
private:
    std::vector<char> buffer;   // 环形缓冲区
    size_t capacity;            // 缓冲区容量
    size_t head = 0;            // 写指针
    size_t tail = 0;            // 读指针
    size_t data_size = 0;       // 当前缓冲区中的数据量

protected:
    void updatePointers();
    auto underflow() -> int override;

public:
    explicit ReaderStreambuf(size_t buffer_size)
        : buffer(buffer_size), capacity(buffer_size) {}

    void write(const char *data, size_t length);

    auto available() const -> size_t { return data_size; }
    auto getCapacity() const -> size_t { return capacity; }
};


class ThreadSafeReaderStreambuf : public ReaderStreambuf
{
private:
    size_t remaining_size = 0;  // 剩余需要读取的数据量
    std::mutex mtx;
    std::condition_variable cv;

protected:
    auto underflow() -> int override;

public:
    explicit ThreadSafeReaderStreambuf(size_t buffer_size)
        : ReaderStreambuf(buffer_size) {}

    void write(const char *data, size_t length);
    void setRemainingSize(size_t remaining_size);
};
