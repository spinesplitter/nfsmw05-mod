#pragma once

// Identical local-plane mapping for native and private-stream mirror vertices.
inline void MirrorVertexUV(const float* pos, const float* b, float* uv) {
    float cu = b[0]*b[3] + b[1]*b[4] + b[2]*b[5];
    float cv = b[0]*b[6] + b[1]*b[7] + b[2]*b[8];
    float pu = pos[0]*b[3] + pos[1]*b[4] + pos[2]*b[5];
    float pv = pos[0]*b[6] + pos[1]*b[7] + pos[2]*b[8];
    float s = b[9]>0 ? (pu-(cu-b[9]))/(2*b[9]) : 0.5f;
    float t = b[10]>0 ? ((cv+b[10])-pv)/(2*b[10]) : 0.5f;
    s = s<0 ? 0 : s>1 ? 1 : s;
    t = t<0 ? 0 : t>1 ? 1 : t;
    uv[0] = b[11] + (b[12]-b[11])*s;
    uv[1] = t;
}