#include "../src/custom_driver.h"
#include <cstdlib>
#include <algorithm>
static void Check(bool pass) { if (!pass) std::exit(1); }
int main() {
    CustomDriver::Asset asset;
    Check(!asset.Load("missing-custom-driver.bin"));
    Check(asset.Load("..\\..\\drivermod\\driver-runtime.bin"));
    Check(asset.vertices.size() > 1500 && asset.vertices.size() % 3 == 0);
    Check(asset.width > 0 && asset.height > 0 && asset.pixels.size() == size_t(asset.width)*asset.height);
    float low = 100, high = -100;
    for (auto v : asset.vertices) { low = std::min(low, v.z); high = std::max(high, v.z); }
    // Raised fingers must survive the neck cut (model Y up to 0.735).
    Check(high > -0.12f && high < -0.10f && low < -0.7f);
    float torsoHigh = -100;
    for (auto v : asset.vertices)
        if (v.x < 0.25f) torsoHigh = std::max(torsoHigh, v.z);
    Check(torsoHigh < -0.1578f && torsoHigh > -0.1580f);
    FILE* bad = fopen("obj\\tests\\bad-driver.bin", "wb");
    Check(bad != nullptr); fputs("CDR1", bad); fclose(bad);
    Check(!asset.Load("obj\\tests\\bad-driver.bin") && asset.vertices.empty() && asset.pixels.empty());
    puts("Custom driver asset, texture payload, head/chin/neck cut and invalid-file tests passed");
}