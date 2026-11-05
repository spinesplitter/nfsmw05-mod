// DRIVERMOD/driver-ik.bin: the driver rig from DRIVERMOD/rigonly.blend, exported by
// vr/tools/export_driver_ik.py.  The rig is already in the in-game driver model's space (x
// forward, y left, z up), so the cockpit mounts it with the driver solid's model-to-world matrix.
// It carries no mesh: the game's driver mesh stays as it is, and `Arms` marks in
// nfsmw_vr_edits.ini select the native mesh triangles driven by the handbrake test.
//
// File layout (little-endian):
//   "IKR1" | version | bone count | control count          (4 x uint32)
//   6 x uint32  bone indices of shoulder, elbow, wrist (left, then right)
//   per bone:  32-byte name | int parent | head xyz | tail xyz | 16-float rest matrix
//   per control: 32-byte name | position xyz
#pragma once
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <utility>
#include <vector>

namespace DriverIK {

constexpr uint32_t kMagic = 0x31524B49u;   // "IKR1"
constexpr uint32_t kVersion = 1;
constexpr int kArmCount = 6;
constexpr uint32_t kMaxBones = 256;
constexpr uint32_t kMaxControls = 64;
constexpr size_t kBoneSize = 32 + 4 + 12 + 12 + 16 * 4;   // name, parent, head, tail, matrix
constexpr size_t kControlSize = 32 + 12;                  // name, position
constexpr size_t kHeaderSize = 16 + kArmCount * 4;

struct Bone {
    char name[32] = {};
    int parent = -1;
    float head[3] = {};
    float tail[3] = {};
    float matrix[16] = {};
    bool arm = false;                                      // shoulder / elbow / wrist (either side)
};

struct Control {
    char name[32] = {};
    float pos[3] = {};
};

struct Rig {
    bool ok = false;
    std::vector<Bone> bones;
    std::vector<Control> controls;
    int arms[kArmCount] = {-1, -1, -1, -1, -1, -1};

    int Find(const char* name) const {
        if (!name) return -1;
        for (size_t i = 0; i < bones.size(); i++)
            if (!std::strncmp(bones[i].name, name, sizeof(bones[i].name))) return (int)i;
        return -1;
    }
};

// Strict parse: rejects a wrong magic/version, a missing or extra long payload, non-positive
// bone counts, out-of-range parents and arm indices.  `size` may be larger than the payload.
inline bool Parse(const void* data, size_t size, Rig* out) {
    if (!out) return false;
    out->ok = false;
    out->bones.clear();
    out->controls.clear();
    if (!data || size < kHeaderSize) return false;
    const uint8_t* p = static_cast<const uint8_t*>(data);
    uint32_t magic = 0, version = 0, boneCount = 0, controlCount = 0;
    std::memcpy(&magic, p, 4); p += 4;
    std::memcpy(&version, p, 4); p += 4;
    std::memcpy(&boneCount, p, 4); p += 4;
    std::memcpy(&controlCount, p, 4); p += 4;
    if (magic != kMagic || version != kVersion) return false;
    if (boneCount == 0 || boneCount > kMaxBones || controlCount > kMaxControls) return false;
    const size_t payload = kHeaderSize + (size_t)boneCount * kBoneSize + (size_t)controlCount * kControlSize;
    if (size < payload) return false;

    Rig rig;
    for (int i = 0; i < kArmCount; i++) {
        uint32_t value = 0;
        std::memcpy(&value, p, 4); p += 4;
        rig.arms[i] = (int)value;
        if (rig.arms[i] < 0 || (uint32_t)rig.arms[i] >= boneCount) return false;
    }
    rig.bones.resize(boneCount);
    for (uint32_t i = 0; i < boneCount; i++) {
        Bone& b = rig.bones[i];
        std::memcpy(b.name, p, 32); p += 32;
        b.name[31] = '\0';
        int32_t parent = -1;
        std::memcpy(&parent, p, 4); p += 4;
        if (parent < -1 || parent >= (int)boneCount) return false;
        b.parent = parent;
        std::memcpy(b.head, p, 12); p += 12;
        std::memcpy(b.tail, p, 12); p += 12;
        std::memcpy(b.matrix, p, 64); p += 64;
    }
    for (int i = 0; i < kArmCount; i++) rig.bones[rig.arms[i]].arm = true;
    rig.controls.resize(controlCount);
    for (uint32_t i = 0; i < controlCount; i++) {
        Control& c = rig.controls[i];
        std::memcpy(c.name, p, 32); p += 32;
        c.name[31] = '\0';
        std::memcpy(c.pos, p, 12); p += 12;
    }
    rig.ok = true;
    *out = std::move(rig);
    return true;
}

inline bool LoadFile(const char* path, Rig* out) {
    if (!path || !out) return false;
    FILE* f = std::fopen(path, "rb");
    if (!f) return false;
    std::fseek(f, 0, SEEK_END);
    long size = std::ftell(f);
    std::fseek(f, 0, SEEK_SET);
    if (size < 0 || (size_t)size > (1u << 20)) { std::fclose(f); return false; }
    std::vector<uint8_t> data((size_t)size);
    bool read = size == 0 || std::fread(data.data(), 1, (size_t)size, f) == (size_t)size;
    std::fclose(f);
    if (!read) return false;
    return Parse(data.data(), data.size(), out);
}

} // namespace DriverIK
