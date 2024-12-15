#include "reader.h"
#include <cstring>
#include <iostream>

// 更新缓冲区指针
void ReaderStreambuf::updatePointers()
{
    if (data_size > 0)
    {
        setg(buffer.data(), buffer.data() + tail, buffer.data() + head);
    }
    else
    {
        setg(buffer.data(), buffer.data(), buffer.data());
    }
}

// 从缓冲区中获取字符
auto ReaderStreambuf::underflow() -> int
{
    if (data_size == 0)
    {
        return traits_type::eof();
    }

    // 从缓冲区读取一个字符
    char current_char = buffer[tail];
    tail = (tail + 1) % capacity;
    --data_size;

    // 更新指针
    setg(buffer.data(), buffer.data() + tail - 1, buffer.data() + tail);

    return traits_type::to_int_type(current_char);
}

// 向缓冲区写入数据
void ReaderStreambuf::write(const char *data, size_t length)
{
    if (length > capacity - data_size)
    {
        throw std::overflow_error("Buffer overflow: not enough space");
    }

    size_t first_chunk = std::min(length, capacity - head);
    std::memcpy(buffer.data() + head, data, first_chunk);
    std::memcpy(buffer.data(), data + first_chunk, length - first_chunk);

    head = (head + length) % capacity;
    data_size += length;
}

// 设置剩余数据量
void ThreadSafeReaderStreambuf::setRemainingSize(size_t remaining_size)
{
    std::unique_lock<std::mutex> lock(mtx);
    this->remaining_size = remaining_size;
    cv.notify_all();
}

// 线程安全的 underflow 实现
auto ThreadSafeReaderStreambuf::underflow() -> int
{
    std::unique_lock<std::mutex> lock(mtx);

    // 等待直到缓冲区有数据或者剩余数据为 0
    cv.wait(lock, [this]() { return available() > 0 || remaining_size == 0; });

    // 如果没有剩余数据，返回 EOF
    if (remaining_size == 0)
    {
        return traits_type::eof();
    }

    // 调用基础类的 underflow 方法
    int result = ReaderStreambuf::underflow();
    if (result != traits_type::eof())
    {
        --remaining_size;
    }

    cv.notify_all();
    return result;
}

// 线程安全的 write 实现
void ThreadSafeReaderStreambuf::write(const char *data, size_t length)
{
    std::unique_lock<std::mutex> lock(mtx);

    // 等待直到有足够的空间写入数据
    cv.wait(lock, [this, length]() { return length <= getCapacity() - available(); });

    // 写入数据到缓冲区
    ReaderStreambuf::write(data, length);

    // 通知读取端有数据可用
    cv.notify_all();
}
