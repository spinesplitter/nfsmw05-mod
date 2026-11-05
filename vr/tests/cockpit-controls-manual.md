# Independent cockpit controls

F4 > VR > Triangle editor: select a connected piece or draw entry, then **Mark as
handbrake** or **Mark as shifter**. Repeat for additional pieces of the same lever.
Marks are saved as `Handbrake=` and `Shifter=` triangle IDs in
`nfsmw_vr_edits.ini`. They are mutually exclusive and exclude those triangles from
steering-wheel detection. Undo restores previous marks; Restore clears them.
Clear handbrake / shifter marks leaves the other editor properties unchanged.

The shifter moves 4.5 cm backward on the bound sequential ShiftUp press and
4.5 cm forward on ShiftDown, with a smooth 0.28 s return-to-neutral stroke.
This currently follows bound sequential press events, not accepted gearbox
changes, automatic shifts, H-pattern selection or the game's unbound shift keys.
The analogue handbrake lifts up to 20 degrees around its lowest marked vertex.
These are model-space defaults, not calibrated per-car lever pivots.

Verify in game:

- Mark every visible piece of each lever; only marked geometry should move.
- Hold/release an analogue handbrake; check intermediate travel and rest position.
- Upshift/downshift repeatedly, reverse direction mid-stroke, and hold a button.
- Check both eyes, mirrors, other cars, and return to the garage.
- Reload the car/game, verify saved marks, Undo, Restore and Clear marks.
- Check wheel-only triangles still follow the existing wheel transform.

This change does not create, rig, spawn or animate a new driver body. It does not
alter the existing driver animation or hide any original car driver models.
Full 900-degree hand-over-hand steering and animated arms/legs remain separate work.