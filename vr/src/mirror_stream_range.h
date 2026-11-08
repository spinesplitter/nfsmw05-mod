#pragma once
#include <stdint.h>

struct MirrorStreamRange {
    unsigned begin = 0, bytes = 0;
    int base = 0;
};

// Preserve index-buffer contents and MinVertexIndex. Only stream zero may be
// rebased; declarations consuming another stream must use the original range.
inline bool MirrorDrawRange(int base, unsigned minimum, unsigned vertices,
                            unsigned offset, unsigned stride, unsigned available,
                            bool singleStream, MirrorStreamRange& range) {
    if (!stride || !vertices) return false;
    const int64_t first = (int64_t)base + minimum;
    const int64_t end = first + vertices;
    if (first < 0 || end <= first || (uint64_t)offset + (uint64_t)end * stride > available)
        return false;
    range.begin = 0;
    range.bytes = available;
    range.base = base;
    if (singleStream && minimum <= 0x7fffffffU) {
        range.begin = (unsigned)((uint64_t)offset + (uint64_t)first * stride);
        range.bytes = vertices * stride;
        range.base = -(int)minimum;
    }
    return true;
}

inline bool MirrorVertexInRange(const MirrorStreamRange& range, int base,
                                unsigned vertex, unsigned offset, unsigned stride) {
    const int64_t index = (int64_t)base + vertex;
    if (index < 0 || !stride) return false;
    const uint64_t begin = (uint64_t)offset + (uint64_t)index * stride;
    return begin >= range.begin && begin + stride <= (uint64_t)range.begin + range.bytes;
}