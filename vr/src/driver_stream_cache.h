#pragma once
#include <stddef.h>
#include <utility>
#include <vector>

// CPU snapshot lifetime is driven by explicit write/release invalidation: the driver and mirror
// streams are rest-pose geometry the game only rewrites through the hooked buffer, so a snapshot
// stays valid until that buffer is written or released. Never cache a partial prefix: a later split
// draw must not introduce another GPU readback.
class DriverStreamCache {
    struct Snapshot {
        void* buffer;
        unsigned offset, stride;
        size_t available;
        std::vector<unsigned char> bytes;
    };
    std::vector<Snapshot> snapshots_;
public:
    struct Stats {
        size_t hits = 0, reads = 0, bytes = 0, failures = 0, invalidations = 0;
    };
private:
    Stats stats_;
    unsigned generation_ = 0;
    static const size_t kMaxSnapshots = 16;   // a live stream per car/glass buffer, with headroom
public:
    const Stats& GetStats() const { return stats_; }
    void ResetStats() { stats_ = Stats{}; }
    // Contents version: bumped whenever a snapshot is dropped. A cached CPU result made from
    // the snapshot is valid only while this is unchanged.
    unsigned Generation() const { return generation_; }
    void Clear() { snapshots_.clear(); ++generation_; }
    // The renderer calls -1 after each eye. It ends timing, not source lifetime.
    void BeginEye(int eye) { if (eye == 0) Clear(); }
    void Invalidate(void* buffer) {
        for (size_t i=0;i<snapshots_.size();)
            if (snapshots_[i].buffer==buffer) {
                ++stats_.invalidations;
                snapshots_.erase(snapshots_.begin()+i);
                ++generation_;
            }
            else ++i;
    }

    // The callback must copy the complete available stream, not drawBytes.
    // A failed read is not retained; callers fall back to the native draw.
    template<class Read>
    const std::vector<unsigned char>* Get(void* buffer, unsigned offset, unsigned stride,
                                          size_t available, size_t drawBytes, Read read) {
        if (!buffer || !stride || !drawBytes || drawBytes>available || drawBytes%stride) return nullptr;
        for (const auto& snapshot : snapshots_)
            if (snapshot.buffer==buffer && snapshot.offset==offset && snapshot.stride==stride &&
                snapshot.available==available) {
                ++stats_.hits;
                return &snapshot.bytes;
            }
        Snapshot snapshot{buffer,offset,stride,available,{}};
        ++stats_.reads;
        if (!read(snapshot.bytes) || snapshot.bytes.size()!=available) {
            ++stats_.failures;
            return nullptr;
        }
        stats_.bytes += snapshot.bytes.size();
        if (snapshots_.size() >= kMaxSnapshots) {
            snapshots_.erase(snapshots_.begin());
            ++generation_;
        }
        snapshots_.push_back(std::move(snapshot));
        return &snapshots_.back().bytes;
    }
};