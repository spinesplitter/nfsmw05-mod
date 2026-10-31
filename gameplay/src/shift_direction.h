#pragma once
#include <math.h>

inline bool FirstGearRollbackAllowed(int gear, float speed) {
    return gear == 2 && fabsf(speed) <= 12.0f / 3.6f;
}