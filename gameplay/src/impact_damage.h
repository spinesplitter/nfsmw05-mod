#pragma once

// Static-world impacts have no other simable. Moving vehicles retain the old
// damage scale; smackable props are rejected by the caller before this point.
inline float ImpactDamage(float speedChange, float minimum, float strength, bool wall) {
    if (!(speedChange > minimum) || !(strength > 0.0f)) return 0.0f;
    return (speedChange - minimum) * 0.01f * strength * (wall ? 1.15f : 1.0f);
}

inline bool CarTotaled(bool destroyed, bool engineBlown) {
    return destroyed || engineBlown;
}