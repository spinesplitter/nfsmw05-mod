#pragma once

#include "vrmath.h"
#include <math.h>

inline bool LimitMirrorFarPlane(M4& projection, float farDistance) {
    if (!(farDistance > 0.0f) || fabsf(projection.m[2][3] - 1.0f) > 1e-5f ||
        fabsf(projection.m[3][3]) > 1e-5f)
        return false;
    const float a = projection.m[2][2];
    const float b = projection.m[3][2];
    if (!(a > 1.0f) || !(b < 0.0f)) return false;
    const float nearDistance = -b / a;
    const float currentFarDistance = -b / (a - 1.0f);
    if (!(nearDistance > 0.0f) || !(currentFarDistance > nearDistance) ||
        farDistance <= nearDistance || farDistance >= currentFarDistance * 0.999f)
        return false;
    projection.m[2][2] = farDistance / (farDistance - nearDistance);
    projection.m[3][2] = -nearDistance * farDistance / (farDistance - nearDistance);
    return true;
}
