#pragma once

#include <cstdint>
#include <type_traits>
#include <memory>

#include "Media.hpp"
#include "MediaShm.hpp"
#include "MediaEfvi.hpp"

template <typename T>
class BasePublisher
{
    static_assert(std::is_trivially_copyable_v<T>);

public:
    BasePublisher() {};

    bool publish(const std::string &topic)
    {
#ifdef USE_EFVI
        media_ = new MediaEfvi<T>(topic, 1024);
        return media_->open();
#else
        media_ = new detail::ShmPub<T>(topic, 1024);
        return media_ != nullptr;
#endif
    }

    size_t write(const T &message) { return media_->write(message); }

    size_t write(const T *message, size_t elem) { return media_->write(message, elem); }

    // template<typename... Args>
    // T *emplace(Args &&...args) 
    // { 
    //     return media_->emplace(std::forward<Args>(args)...);
    // }

    void close(bool flush) { media_.close(flush); }

private:
    Media<T> *media_;
    //std::unique_ptr<Media<T>> media_;
};