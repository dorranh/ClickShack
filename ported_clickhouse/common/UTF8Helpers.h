#pragma once

#include <cstdint>

inline const char * skipWhitespacesUTF8(const char * pos, const char * end)
{
    while (pos < end)
    {
        if (*pos == ' ' || *pos == '\t' || *pos == '\n' || *pos == '\r' || *pos == '\f' || *pos == '\v')
            ++pos;
        else
            break;
    }
    return pos;
}

namespace UTF8
{
inline bool isContinuationOctet(unsigned char octet)
{
    return (octet & 0b11000000u) == 0b10000000u;
}
}
