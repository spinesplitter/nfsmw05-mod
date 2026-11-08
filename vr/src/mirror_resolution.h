#pragma once

struct MirrorResolutionRect {
    unsigned x, y, width, height;
};

inline bool ScaleMirrorResolutionRect(unsigned targetWidth, unsigned targetHeight, unsigned x, unsigned y,
                                      unsigned width, unsigned height, float scale, MirrorResolutionRect& out) {
    if (!targetWidth || !targetHeight || !width || !height || !(scale > 0.0f) || scale >= 1.0f ||
        x >= targetWidth || y >= targetHeight)
        return false;
    const unsigned scaledTargetWidth = (unsigned)(targetWidth * scale);
    const unsigned scaledTargetHeight = (unsigned)(targetHeight * scale);
    const unsigned maxWidth = scaledTargetWidth ? scaledTargetWidth : 1;
    const unsigned maxHeight = scaledTargetHeight ? scaledTargetHeight : 1;
    out.x = (unsigned)(x * scale);
    out.y = (unsigned)(y * scale);
    out.width = (unsigned)(width * scale);
    out.height = (unsigned)(height * scale);
    if (!out.width) out.width = 1;
    if (!out.height) out.height = 1;
    if (out.x >= maxWidth) out.x = maxWidth - 1;
    if (out.y >= maxHeight) out.y = maxHeight - 1;
    if (out.width > maxWidth - out.x) out.width = maxWidth - out.x;
    if (out.height > maxHeight - out.y) out.height = maxHeight - out.y;
    return out.width && out.height;
}
