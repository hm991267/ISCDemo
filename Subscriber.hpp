#pragma once

#include <cstdint>
#include <functional>
#include <type_traits>

#include "Media.hpp"

#include "MediaShm.hpp"

template <typename T>
class BaseSubscriber
{
    static_assert(std::is_trivially_copyable_v<T>);
    using Callback = std::function<void(T *, size_t elem)>;

public:
    BaseSubscriber() {}

    bool subscribe(const std::string &topic)
    { 
        media_ = new detail::ShmSub<T>(topic, 1024);
        return media_ != nullptr;
    }
    bool subscribe(const std::string &topic, Callback &&callback)
    {
        callback_ = callback;
        return subscribe(topic);
    }

    size_t read(T &message) { return media_->read(message); }

    // template <size_t N>
    // size_t read(T (&message)[N])
    // {
    //     return media_->read(message);
    // }

    void poll() {}

    void close() {}

private:
    Callback callback_;
    Media<T> *media_;
};