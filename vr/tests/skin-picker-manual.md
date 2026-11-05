# Native driver skin material override

## Skin UV repair and rendering cost

Skin-selected and automatically classified finger vertices decode V in [-1,0)
by adding 1 in a private stream copy before arm posing. Positive UVs, other
attributes, original buffers and global sampler state remain unchanged. Explicit
Black excludes automatic skin repair. This also works without active steering.
Tests cover selection isolation, boundaries, idempotence and Black precedence;
skin colour and motion still require in-game confirmation on the affected cars.

The temporary GPU input capture and skin-binding diagnostic logs have been
removed. No textures, shaders or vertex data are dumped during normal play.
Existing local captures are proprietary and must not be committed or distributed.
Earlier capture interpretations are not treated here as independently verified.

Unselected, non-automatic ranges bypass GPU readback. Split driver ranges reuse
one read-only stream snapshot per buffer/offset/stride per eye; indices come from
the live CPU mesh, with validated base-vertex remapping, rather than locking the
GPU index buffer for every draw. The snapshot is discarded at each eye boundary
so steering poses are always applied to fresh source data, never accumulated.
This targets synchronization stalls introduced by the broader UV-repair draw
gate; an in-game frame-time comparison is still needed to quantify improvement.

The main driver's resolved shader, five texture bindings and auxiliary binding
are retained for selected skin runs, including when its geometry is hidden.
Saved selections, Hand fix and arm posing are preserved.

## Automatic separate-finger fix

Secondary native driver entries classified by Prepare as mostly hands/arms
automatically borrow the main entry's skin shader and texture bindings. This is
shared across car models and LODs: no picker marks or per-car setup are needed.
The material is read from the same live solid on each submission, never another
car. Copied animated/hidden-head ranges are covered; the bounded skin UV repair precedes existing arm animation. Explicit Black marks still take precedence over automatic skin.
These entries are also automatically included in arm deformation and shown when
the native arms are hidden. Neither Arms nor Show triangle marks are required.
Pick the finger triangles and press **Merge fingers** in the triangle editor.
The command uses exactly the highlighted driver selection, like Mark as arms.
Choose whole draw entry to merge the whole finger submesh, or a smaller brush
to merge only part of it. Entry numbers are not hard-coded. It saves Arms,
Skin and Show together and removes conflicting Black/Hide marks only there.
Non-driver selections are rejected. Undo reverses the entire merge in one step.
There is no longer an unconditional second-entry update on car load. Existing
saved marks are retained. This command reuses Draw as skin's material route;
its in-game colour still needs confirmation on the failing Gallardo.

Check all 36 cars in-game, including LOD changes and returning to a previous car:
fingers should match the main driver skin in both eyes, at neutral, full lock,
shifting and handbrake. Non-hand secondary materials must remain unchanged.
Automated tests cover classification/material routing, not visual UV correctness
or every proprietary car asset. A separately named solid not recognised as a
driver or a non-arm-classified detail needs investigation rather than broad
recolouring of every material.

## Optional manual override

In F4's triangle picker, select only the black finger/detail pieces and choose
**Draw as skin**. This borrows the first submesh's skin atlas and shader, retaining
the selected geometry's normals and IK deformation, with the bounded V repair. The atlas
must already contain the finger skin island expected by those UVs; this does not
paint or generate new UVs/textures.

- Check fingers in both eyes at neutral, both steering locks, shifting and handbrake.
- Confirm adjacent unselected triangles keep their original materials.
- Undo must restore the old marking. Restore clears the skin override.
- Restart the game and check that the selection persists (the `Skin` entry in
  the existing per-solid triangle-edits INI).
- No external texture or proprietary rig is copied into the source tree.