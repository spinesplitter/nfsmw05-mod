#pragma once

inline bool MirrorPatchWanted(bool patchOk, bool mirrors, bool disabled, bool xrAvailable) {
    return patchOk && mirrors && !disabled && xrAvailable;
}

inline bool SuppressMirrorGlassDraw(bool disabled, bool registered, unsigned glassFirst, unsigned drawFirst) {
    return disabled && registered && glassFirst == drawFirst;
}
