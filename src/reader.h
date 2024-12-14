#pragma once

#include <condition_variable>
#include <mutex>
#include <streambuf>
#include <vector>

class ReaderStreambuf : public std::streambuf
{
private:
    std::vector<char> buffer; // 环形缓冲区
    size_t capacity;          // 缓冲区容量
    size_t head = 0;          // 写指针
    size_t tail = 0;          // 读指针
    size_t data_size = 0;     // 当前缓冲区中的数据量
    bool eof = false;         // 是否已经读到文件末尾

    void updatePointers();

protected:
    auto underflow() -> int override;

public:
    explicit ReaderStreambuf(size_t buffer_size)
        : buffer(buffer_size), capacity(buffer_size) {}

    void write(const char *data, size_t length);

    [[nodiscard]] auto available() const -> size_t { return data_size; }

    // Getter for capacity
    [[nodiscard]] auto getCapacity() const -> size_t { return capacity; }

    void setEof(bool eof) { this->eof = eof; }
};

class ThreadSafeReaderStreambuf : public ReaderStreambuf
{
private:
    std::mutex mtx;
    std::condition_variable cv;

public:
    explicit ThreadSafeReaderStreambuf(size_t buffer_size)
        : ReaderStreambuf(buffer_size) {}

    void write(const char *data, size_t length);
};
