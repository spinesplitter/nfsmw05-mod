// INI-only adapter for the separate, unmodified ExOpts plugin. No runtime hooks.
#pragma once
#include <windows.h>
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <string>

namespace extra_options {
constexpr struct Setting {
    const char* section;
    const char* key;
    float def, lo, hi;
} settings[] = {
    {"OpponentControllers", "Maximum", 15, 0, 15},
    {"OpponentControllers", "RandomRace", 6, 0, 15},
    {"LapControllers", "Maximum", 127, 0, 127},
    {"TrafficControllers", "Low", 10, 0, 100},
    {"TrafficControllers", "Medium", 30, 0, 100},
    {"TrafficControllers", "High", 100, 0, 100},
};
constexpr int count = sizeof(settings) / sizeof(settings[0]);

inline float Read(const std::string& path, int i) {
    const auto& s = settings[i];
    char text[128];
    GetPrivateProfileStringA(s.section, s.key, "", text, sizeof(text), path.c_str());
    char* end = nullptr;
    float value = strtof(text, &end);
    // Upstream INIs use inline // comments. Missing/invalid values use our profile default.
    if (end == text || !std::isfinite(value)) return s.def;
    return std::max(s.lo, std::min(s.hi, roundf(value)));
}
}  // namespace extra_options