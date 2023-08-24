#pragma once

#include <cstddef>
#include <cstdint>

#include "EfviWrapper.hpp"
#include "Media.hpp"

template <typename T>
class MediaEfvi : public Media<T>
{
public:
    bool registerTopic(const TopicInfo &topic) { impl_.init(); }

    // copy single
    size_t publish(const T &message) { return impl_.transmit(&message, sizeof(T)); }

    // copy batch
    size_t publish(const T *message, size_t elem) { return impl_.transmitBatch(&message, elem * sizeof(T)); }

    // zero copy
    T *beginPublish(size_t elem)  { return impl_.beginTransmit(elem, sizeof(T)); }
   
    void endPublish() { return impl_.endTransmit(); }

    void close(bool flush) {}

private:
    EfviWrapper impl_;
};