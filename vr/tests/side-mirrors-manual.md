# Side mirror view assignment

The rear-facing camera's high-u side is the car's left. All custom side mirror
paths use SideMirrorRange: separate mirror solids, body glass with its own draw
entry, shared entries, and split-out glass (including Carrera GT and Camaro
triangle selections). MirrorFlip reverses each crop's endpoints, not its side.

Automated cockpit tests check both sides with MirrorFlip on/off and crop widths
0.2, 0.6 and 1.0. Existing saved geometry diagnostics confirm correctly signed
left/right glass positions for Camaro, Carrera GT, Cayman S, Monaro, Punto, RX-8.
This is not an exhaustive visual validation of every installed model or body kit.

Live validation for each drivable model and available body kit:

1. Park near a distinguishable object behind the LEFT side of the car. It should
   appear in the left mirror, not solely in the right one. Repeat on the right.
2. Test MirrorFlip=0 and 1: horizontal orientation changes, not which side appears.
3. Verify glass position, outline and housing are unchanged; centre mirror is
   unchanged. Width=1 intentionally shows the entire rear camera in both mirrors.
4. Change car, body kit and camera, pause/resume, and repeat in the headset.
5. GameMirrorsSkip cars retain their stock mirrors and are outside this change.