#pragma once
#include <stddef.h>
#include <stdint.h>
#include <vector>

// Keep vertex indices unchanged: copy only the prefix they can address.
// Division before multiplication also bounds overflow for malformed streams.
inline size_t DriverStreamBytes(const std::vector<uint16_t>& indices, unsigned stride, size_t available) {
    if (!stride || indices.empty()) return 0;
    size_t count = 0;
    for (uint16_t index : indices)
        if ((size_t)index + 1 > count) count = (size_t)index + 1;
    return count <= available / stride ? count * stride : 0;
}