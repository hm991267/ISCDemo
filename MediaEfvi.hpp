#pragma once

#include <cstddef>
#include <cstdint>

#include "EfviWrapper.hpp"
#include "Media.hpp"

template <typename T>
class MediaEfvi : public Media<T>
{
public:
    MediaEfvi(const std::string &name, size_t bufferSize)
        : Media<T>(name, bufferSize, false)
    {
    }

    bool open() override { return impl_.init(this->endpoint_, sizeof(T)); }

    // copy single
    size_t write(const T &message) override { return impl_.transmit((uint8_t *)&message, sizeof(T)); }

    // copy batch
    size_t write(const T *message, size_t elem) override
    {
        return impl_.transmit((uint8_t *)message, elem * sizeof(T));
    }

    size_t read(T &message) override { return impl_.receive((uint8_t *)&message, sizeof(T)); }

    size_t read(T *message, size_t elem) override { return impl_.receive((uint8_t *)message, elem * sizeof(T)); }

    void poll(typename Media<T>::Callback&& callback) override { impl_.poll([&](uint8_t* buf){ callback(*(T*)buf); }); }

    void close(bool flush) override {}

private:
    EfviWrapper impl_;
};