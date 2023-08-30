#pragma once

#include <cstddef>
#include <cstdint>
#include <cassert>
#include <string>
#include <functional>

struct TopicPacket {
    int64_t pubTime;
    int32_t totalSize;
    int16_t fragment;
    int16_t fragId;
};

// topic format
// file://test
// s5p1|10.10.1.54:50000

template<typename T>
class Media
{
public:
    using Callback = std::function<void(const T&)>;

    Media(const std::string& topic, size_t buffer, bool readOnly)
        : endpoint_(topic)
        , bufferSize_(buffer)
        , readOnly_(readOnly)
    {
    }

    virtual bool open() { return 0; }

    virtual size_t write(const T &message) { assert(0); }

    virtual size_t write(const T *message, size_t elem) { assert(0); }

    // template<typename... Args>
    // virtual inline size_t emplace(Args &&... args) {
    //     return 0;
    // }

    virtual size_t read(T &message) { assert(0); }

    // template<size_t N>
    // virtual inline size_t read(T (&message)[N]) {
    //     return 0;
    // }
    virtual size_t read(T *message, size_t elem) { assert(0); }

    virtual void poll(Callback &&callback) { assert(0); }

    virtual void close(bool flush) {}
protected:
    const std::string endpoint_;
    size_t bufferSize_;
    bool readOnly_;
};
