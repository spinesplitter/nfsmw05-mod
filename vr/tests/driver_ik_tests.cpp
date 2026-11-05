// DRIVERMOD/driver-ik.bin (the driver rig exported from rigonly.blend): strict loading.
#include "../src/driver_ik.h"
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <vector>

static void Check(bool pass) { if (!pass) std::exit(1); }

struct Writer {
    std::vector<unsigned char> b;
    void u32(uint32_t v) { for (int i = 0; i < 4; i++) b.push_back((unsigned char)(v >> (8 * i))); }
    void i32(int32_t v) { u32((uint32_t)v); }
    void f32(float v) { uint32_t u; std::memcpy(&u, &v, 4); u32(u); }
    void f3(float x, float y, float z) { f32(x); f32(y); f32(z); }
    void name(const char* n) {
        char buf[32] = {};
        std::strncpy(buf, n, 31);
        for (int i = 0; i < 32; i++) b.push_back((unsigned char)buf[i]);
    }
    void identity() { for (int i = 0; i < 16; i++) f32(i % 5 == 0 ? 1.0f : 0.0f); }
};

static std::vector<unsigned char> MakeRig() {
    static const char* kNames[16] = {
        "JNT_spine_01", "JNT_spine_02", "JNT_chest", "JNT_clavicle.L", "JNT_shoulder.L",
        "JNT_elbow.L", "JNT_wrist.L", "JNT_hand_tip.L", "JNT_neck", "JNT_head",
        "JNT_head_tip", "JNT_clavicle.R", "JNT_shoulder.R", "JNT_elbow.R", "JNT_wrist.R",
        "JNT_hand_tip.R"};
    Writer w;
    w.u32(DriverIK::kMagic);
    w.u32(DriverIK::kVersion);
    w.u32(16);
    w.u32(2);
    w.u32(4);  w.u32(5);  w.u32(6);      // left shoulder, elbow, wrist
    w.u32(12); w.u32(13); w.u32(14);     // right shoulder, elbow, wrist
    for (int i = 0; i < 16; i++) {
        w.name(kNames[i]);
        w.i32(i == 0 ? -1 : i - 1);
        w.f3(0.1f * i, 0.2f * i, 0.3f * i);
        w.f3(0.1f * i + 0.05f, 0.2f * i, 0.3f * i);
        w.identity();
    }
    w.name("CTRL_SteeringHand.L");
    w.f3(0.5f, 0.3f, 0.2f);
    w.name("CTRL_SteeringHand.R");
    w.f3(0.5f, -0.3f, 0.2f);
    return w.b;
}

int main(int argc, char** argv) {
    std::vector<unsigned char> good = MakeRig();

    DriverIK::Rig rig;
    Check(DriverIK::Parse(good.data(), good.size(), &rig));
    Check(rig.ok && rig.bones.size() == 16 && rig.controls.size() == 2);
    Check(rig.Find("JNT_elbow.L") == 5 && rig.Find("JNT_wrist.R") == 14);
    Check(rig.Find("missing") == -1);
    Check(rig.bones[5].arm && rig.bones[14].arm && !rig.bones[2].arm);
    Check(rig.controls[0].name[0] == 'C' && rig.controls[1].pos[1] < 0);

    // An exporter may round the payload up: extra bytes after it are fine.
    std::vector<unsigned char> padded = good;
    padded.push_back(0);
    Check(DriverIK::Parse(padded.data(), padded.size(), &rig));

    // Truncated, wrong magic and wrong version are rejected.
    Check(!DriverIK::Parse(good.data(), good.size() - 1, &rig) && !rig.ok);
    std::vector<unsigned char> bad = good;
    bad[0] = 'X';
    Check(!DriverIK::Parse(bad.data(), bad.size(), &rig));

    // An arm index past the bone list is rejected.
    bad = good;
    size_t arm0 = 16;
    bad[arm0 + 0] = 0xFF;
    bad[arm0 + 1] = 0xFF;
    Check(!DriverIK::Parse(bad.data(), bad.size(), &rig));

    // A parent past the bone list is rejected.
    bad = good;
    size_t parent = DriverIK::kHeaderSize + 2 * DriverIK::kBoneSize + 32;   // bone 2's parent
    bad[parent + 0] = 0xFF;
    bad[parent + 1] = 0xFF;
    Check(!DriverIK::Parse(bad.data(), bad.size(), &rig));

    // No bones is not a rig.
    bad = good;
    bad[8] = 0;
    Check(!DriverIK::Parse(bad.data(), bad.size(), &rig));

    Check(!DriverIK::LoadFile("obj\\tests\\missing-driver-ik.bin", &rig));

    // When built against a real game copy, check the exported rig itself.
    if (argc > 1) {
        Check(DriverIK::LoadFile(argv[1], &rig));
        Check(rig.bones.size() >= 10 && rig.controls.size() == 2);
        for (int i = 0; i < DriverIK::kArmCount; i++) Check(rig.bones[rig.arms[i]].arm);
        puts("Real driver-ik.bin loaded");
    }
    puts("Driver IK asset and invalid-file tests passed");
}
