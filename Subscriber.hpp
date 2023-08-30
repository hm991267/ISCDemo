#pragma once

#include <cstdint>
#include <functional>
#include <type_traits>

#include "Media.hpp"

#include "MediaShm.hpp"
#include "MediaEfvi.hpp"

template <typename T>
class BaseSubscriber
{
    static_assert(std::is_trivially_copyable_v<T>);
    using Callback = typename Media<T>::Callback;

public:
    BaseSubscriber() {}

    bool subscribe(const std::string &topic)
    {
#ifdef USE_EFVI
        media_ = new MediaEfvi<T>(topic, 1024);
        return media_->open();
#else
        media_ = new detail::ShmSub<T>(topic, 1024);
        return media_ != nullptr;
#endif
    }

    bool subscribe(const std::string &topic, Callback &&callback)
    {
        callback_ = callback;
        return subscribe(topic);
    }

    size_t read(T &message) { return media_->read(message); }

    size_t read(T *message, size_t elem) { return media_->read(message, elem); }

    // template <size_t N>
    // size_t read(T (&message)[N])
    // {
    //     return media_->read(message);
    // }

    void poll() { media_->poll(callback_); }

    void close() {}

private:
    Callback callback_;
    Media<T> *media_;
};