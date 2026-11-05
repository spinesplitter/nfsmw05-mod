# Native driver skin material override

In F4's triangle picker, select only the black finger/detail pieces and choose
**Draw as skin**. This borrows the first submesh's skin atlas and shader, retaining
the selected geometry's UVs, normals, vertex buffers and IK deformation. The atlas
must already contain the finger skin island expected by those UVs; this does not
paint or generate new UVs/textures.

- Check fingers in both eyes at neutral, both steering locks, shifting and handbrake.
- Confirm adjacent unselected triangles keep their original materials.
- Undo must restore the old marking. Restore clears the skin override.
- Restart the game and check that the selection persists (the `Skin` entry in
  the existing per-solid triangle-edits INI).
- No external texture or proprietary rig is copied into the source tree.