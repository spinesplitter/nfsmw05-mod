# Native fullscreen spectator

With VR active and `ForceWindowed=1`, set `SpectatorNativeFullscreen=1` in
`[VR]` to make the desktop spectator fill the current monitor. The option is
off by default. `SpectatorWidth` and the
headset/configured spectator aspect continue to define the legacy HUD canvas;
only the desktop projection uses the monitor aspect.

The window remains a real fullscreen client. Its `WM_SIZE` messages are
translated to the dimensions the prior spectator window would have used, so
the widescreen fix does not resize HUD artwork to the desktop aspect before
the VR HUD is cropped. Minimize, input, focus and destruction messages still
reach the original window procedure. This does not virtualize `GetClientRect`
for other plugins.

Restart the game after changing the setting. Verify the headset HUD and desktop
spectator in game before release; window and renderer tests cannot confirm the
runtime appearance.
