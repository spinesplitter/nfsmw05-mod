#pragma once
#include <stddef.h>
#include <utility>
#include <vector>

// CPU snapshot lifetime is exactly one eye/present. Never cache a partial
// prefix: a later split draw must not introduce another GPU readback.
class DriverStreamCache {
    struct Snapshot {
        void* buffer;
        unsigned offset, stride;
        size_t available;
        std::vector<unsigned char> bytes;
    };
    std::vector<Snapshot> snapshots_;
public:
    void Clear() { snapshots_.clear(); }

    // The callback must copy the complete available stream, not drawBytes.
    // A failed read is not retained; callers fall back to the native draw.
    template<class Read>
    const std::vector<unsigned char>* Get(void* buffer, unsigned offset, unsigned stride,
                                          size_t available, size_t drawBytes, Read read) {
        if (!buffer || !stride || !drawBytes || drawBytes>available || drawBytes%stride) return nullptr;
        for (const auto& snapshot : snapshots_)
            if (snapshot.buffer==buffer && snapshot.offset==offset && snapshot.stride==stride &&
                snapshot.available==available) return &snapshot.bytes;
        Snapshot snapshot{buffer,offset,stride,available,{}};
        if (!read(snapshot.bytes) || snapshot.bytes.size()!=available) return nullptr;
        snapshots_.push_back(std::move(snapshot));
        return &snapshots_.back().bytes;
    }
};