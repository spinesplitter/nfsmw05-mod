# Driver readback / RTSS verification

The driver source snapshot is shared by split draws and the second eye, not by
successive frames. Writable locks invalidate all ranges for that source; read-only
locks do not. Final Release invalidates identity before address reuse. Reset and
ProcessVertices also invalidate. Offset, stride and available length remain part
of the key. Failed hook installation or later hook replacement disables reuse for
that source draw. Posed vertices and repaired UVs remain private per-draw copies.

The buffer hooks follow the existing render-thread ownership of this renderer;
this change does not introduce support for concurrent resource writes/draws.
Cross-frame reuse is deliberately not enabled.

Automated coverage:
- driver_stream_cache_tests.h: cross-eye reuse, writes, address reuse, independent
  sources, offsets, strides, sizes, invalid ranges and read failures.
- driver_buffer_hooks_tests.cpp: actual COM slot dispatch through fake vtables,
  read-only versus writable locks, final/non-final Release, multiple vtables and
  detection of later hook replacement.
- Existing pose tests cover live rig/vertex mutation, normals and editor changes.

Before collecting the next controlled trace, verify in game:
1. Both eyes show animated arms, steering and shifting, with no stale geometry.
2. Change an editor selection, reload a car and switch camera modes.
3. Check driver readback detail time against a same-route baseline. A write
   between eyes legitimately requires another read; no measured gain is promised.
4. Check Afterburner FPS monitoring and RTSS OSD on the desktop spectator window.
   The mod now presents its Ex device through PresentEx(flags=0), allowing an Ex
   hook to observe presentation. This does not implement an OpenXR headset OSD.
   Runtime confirmation is required; FPS measured here is desktop application
   presentation, not headset compositor/reprojection FPS.

No Afterburner tuning, RTSS global profile, frame limiter or OpenXR wait settings
are changed by this patch.