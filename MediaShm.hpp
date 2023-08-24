#pragma once

#include <cstddef>
#include <cstdint>

#include "Media.hpp"
#include <hammer/ipc/Ringbuffer.hpp>

namespace detail
{

template <typename T>
class ShmPub : public Media<T>
{
public:
    ShmPub(const std::string &name, size_t bufferSize) 
        : Media<T>(name, bufferSize, false)
        , shmFile_(name)
        , ringBuff_(name.c_str(), bufferSize, true)
    {}

    // copy single
    size_t write(const T &message) override
    {
        ringBuff_.push(message);
        return 1;
    }

    // copy batch
    size_t write(const T *message, size_t elem) override
    {
        for (size_t i = 0; i < elem; i++) {
            ringBuff_.push(message[i]);
        }
        return elem;
    }

    // zero copy
    // template<typename... Args>
    // size_t emplace(Args &&...args) override
    // {
    //     ringBuff_.emplace(std::forward<Args>(args)...);
    //     return 1;
    // }

    void close(bool flush) {}

private:
    const std::string shmFile_;
    hammer::ipc::ring_buffer<T> ringBuff_;
};

template <typename T>
class ShmSub : public Media<T>
{
public:
    ShmSub(const std::string &name, size_t bufferSize) 
        : Media<T>(name, bufferSize, true)
        , shmFile_(name)
        , ringBuff_(name.c_str())
    {}

    // copy single
    size_t read(T &message) override
    {
        message = ringBuff_.get();
        ringBuff_.next();
        return 1;
    }

    // copy batch
    // template<size_t N>
    // size_t read(T (&message)[N]) override
    // {
    //     size_t i;
    //     for (i = 0; i < N;)
    //     {
    //         bool result = ringBuff_.get_nonblock(message[i]);
    //         if (result)
    //             i++;
    //         else
    //             break;
    //     }
    //     return i;
    // }

    void close(bool flush) override {}

private:
    std::string shmFile_;
    hammer::ipc::ring_buffer_reader<T> ringBuff_;
};

}