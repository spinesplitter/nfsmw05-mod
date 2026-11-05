#pragma once
#include <cstdio>
#include <cstdint>
#include <vector>
#include <cmath>

namespace CustomDriver {
struct Vertex { float x, y, z, u, v; };
struct Asset {
    std::vector<Vertex> vertices;
    std::vector<uint32_t> pixels;
    uint32_t width = 0, height = 0;
    bool Load(const char* path) {
        vertices.clear(); pixels.clear(); width = height = 0;
        FILE* file = fopen(path, "rb");
        if (!file) return false;
        uint32_t header[4] = {};
        bool ok = fread(header, sizeof(header), 1, file) == 1 && header[0] == 0x31524443 &&
            header[1] > 0 && header[1] <= 300000 && header[1] % 3 == 0 &&
            header[2] > 0 && header[2] <= 4096 && header[3] > 0 && header[3] <= 4096;
        if (ok) {
            vertices.resize(header[1]); pixels.resize(size_t(header[2])*header[3]);
            ok = fread(vertices.data(), sizeof(Vertex), vertices.size(), file) == vertices.size() &&
                fread(pixels.data(), sizeof(uint32_t), pixels.size(), file) == pixels.size() && fgetc(file) == EOF;
            for (const auto& v : vertices) ok = ok && std::isfinite(v.x) && std::isfinite(v.y) &&
                std::isfinite(v.z) && std::isfinite(v.u) && std::isfinite(v.v);
        }
        fclose(file);
        if (!ok) { vertices.clear(); pixels.clear(); return false; }
        width = header[2]; height = header[3]; return true;
    }
};
}