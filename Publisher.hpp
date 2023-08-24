#pragma once

#include <cstdint>
#include <type_traits>
#include <memory>

#include "Media.hpp"
#include "MediaShm.hpp"

template <typename T>
class BasePublisher
{
    static_assert(std::is_trivially_copyable_v<T>);

public:
    BasePublisher() {};

    bool publish(const std::string &topic)
    {
        media_ = new detail::ShmPub<T>(topic, 1024);
        return media_ != nullptr;
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