#pragma once

#include <cstdint>

struct MessageHeader
{
    int64_t clock;
    int32_t sequenceId;
    int16_t segamentId : 3; // max 8 segments, actually 8192 / 1500 = 6 segments
    int16_t length : 13;    // max length = 8192
};