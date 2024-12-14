#include "reader.h"
#include <cstring>

void ReaderStreambuf::updatePointers()
{
    if (data_size > 0)
    {
        // 更新输入缓冲区指针
        setg(buffer.data(), buffer.data() + tail, buffer.data() + head);
    }
    else
    {
        // 如果没有数据可读，设置为空缓冲区
        setg(buffer.data(), buffer.data(), buffer.data());
    }
}

auto ReaderStreambuf::underflow() -> int
{
retry:
    if (data_size == 0)
    {
        if (eof)
        {
            return traits_type::eof();
        }
        else
        {
            goto retry;
        }
    }

    // 自动消耗一个字符
    char current_char = buffer[tail];
    tail = (tail + 1) % capacity;
    --data_size;

    // 更新指针，使 istream 能够感知当前字符
    setg(buffer.data(), buffer.data() + tail - 1, buffer.data() + tail);

    return traits_type::to_int_type(*gptr());
}

void ReaderStreambuf::write(const char *data, size_t length)
{
    if (length > capacity - data_size)
    {
        throw std::overflow_error("Buffer overflow: not enough space");
    }

    size_t first_chunk = std::min(length, capacity - head);
    memcpy(buffer.data() + head, data, first_chunk);
    memcpy(buffer.data(), data + first_chunk, length - first_chunk);

    head = (head + length) % capacity;
    data_size += length;
}

void ThreadSafeReaderStreambuf::write(const char *data, size_t length)
{
    std::unique_lock<std::mutex> lock(mtx);
    while (length > getCapacity() - available())
    {
        cv.wait(lock); // 等待空间可用
    }
    ReaderStreambuf::write(data, length);
    cv.notify_all(); // 通知读取方
}
