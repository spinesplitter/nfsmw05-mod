#pragma once

struct MirrorVertexMapper {
    float u[3], v[3];
    float uStart, uInvWidth, vEnd, vInvHeight;
    float u0, uRange;
    bool hasWidth, hasHeight;

    explicit MirrorVertexMapper(const float* b)
        : u{b[3], b[4], b[5]}, v{b[6], b[7], b[8]},
          uStart(0.0f), uInvWidth(0.0f), vEnd(0.0f), vInvHeight(0.0f),
          u0(b[11]), uRange(b[12] - b[11]), hasWidth(b[9] > 0.0f), hasHeight(b[10] > 0.0f) {
        float cu = b[0] * u[0] + b[1] * u[1] + b[2] * u[2];
        float cv = b[0] * v[0] + b[1] * v[1] + b[2] * v[2];
        if (hasWidth) {
            uStart = cu - b[9];
            uInvWidth = 1.0f / (2.0f * b[9]);
        }
        if (hasHeight) {
            vEnd = cv + b[10];
            vInvHeight = 1.0f / (2.0f * b[10]);
        }
    }

    // Precompute the basis projections once per glass part, not once per vertex.
    inline void Map(const float* pos, float* uv) const {
        float pu = pos[0] * u[0] + pos[1] * u[1] + pos[2] * u[2];
        float pv = pos[0] * v[0] + pos[1] * v[1] + pos[2] * v[2];
        float s = hasWidth ? (pu - uStart) * uInvWidth : 0.5f;
        float t = hasHeight ? (vEnd - pv) * vInvHeight : 0.5f;
        s = s < 0 ? 0 : s > 1 ? 1 : s;
        t = t < 0 ? 0 : t > 1 ? 1 : t;
        uv[0] = u0 + uRange * s;
        uv[1] = t;
    }
};

// Identical local-plane mapping for native and private-stream mirror vertices.
inline void MirrorVertexUV(const float* pos, const float* b, float* uv) {
    MirrorVertexMapper(b).Map(pos, uv);
}
