"""Export DRIVERMOD/rigonly.blend's armature to DRIVERMOD/driver-ik.bin.

Run with Blender in background mode:

    blender.exe -b DRIVERMOD/rigonly.blend --python vr/tools/export_driver_ik.py

The rig is saved in driver-model space (x forward, y left, z up).  It contains
no mesh or skin weights: the game driver's own mesh remains the visible model.
`Arms` in nfsmw_vr_edits.ini selects the driver triangles that the later
animation step will drive with these bones.
"""
import bpy
import json
import struct
import sys
from pathlib import Path
from mathutils import Vector

MAGIC = b"IKR1"
VERSION = 1


def arg_value(name, default=None):
    args = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
    for i, arg in enumerate(args):
        if arg == name and i + 1 < len(args):
            return args[i + 1]
        if arg.startswith(name + "="):
            return arg.split("=", 1)[1]
    return default


def packed_name(name):
    raw = name.encode("utf-8")[:31]
    return raw + b"\0" * (32 - len(raw))


def find_bone(armature, side, role):
    exact = (
        f"JNT_{role}.{side}",
        f"{role}.{side}",
        f"Bip01 {side} {role}",
    )
    lowered = {b.name.lower(): b for b in armature.data.bones}
    for name in exact:
        if name.lower() in lowered:
            return lowered[name.lower()]
    for b in armature.data.bones:
        n = b.name.lower()
        if role.lower() in n and n.endswith("." + side.lower()):
            return b
    raise ValueError(f"arm bone not found: {role}.{side}")


def main():
    blend = Path(bpy.data.filepath)
    output = Path(arg_value("--output", str(blend.parent / "driver-ik.bin")))
    armatures = [o for o in bpy.data.objects if o.type == "ARMATURE"]
    if len(armatures) != 1:
        raise ValueError(f"expected exactly one armature, found {len(armatures)}")
    arm = armatures[0]
    bones = list(arm.data.bones)
    index = {b.name: i for i, b in enumerate(bones)}

    arms = []
    for side in ("L", "R"):
        for role in ("shoulder", "elbow", "wrist"):
            arms.append(index[find_bone(arm, side, role).name])

    controls = [o for o in bpy.data.objects if o.type == "EMPTY" and o.name.startswith("CTRL_")]
    controls.sort(key=lambda o: o.name)

    with output.open("wb") as f:
        f.write(struct.pack("<4sIII", MAGIC, VERSION, len(bones), len(controls)))
        f.write(struct.pack("<6I", *arms))
        for bone in bones:
            parent = index[bone.parent.name] if bone.parent else -1
            head = arm.matrix_world @ bone.head_local
            tail = arm.matrix_world @ bone.tail_local
            matrix = arm.matrix_world @ bone.matrix_local
            f.write(packed_name(bone.name))
            f.write(struct.pack("<i3f3f16f", parent, *head, *tail,
                                *(matrix[r][c] for r in range(4) for c in range(4))))
        for obj in controls:
            pos = arm.matrix_world.inverted() @ obj.matrix_world.translation
            f.write(packed_name(obj.name))
            f.write(struct.pack("<3f", *pos))

    report = {
        "output": str(output),
        "source": str(blend),
        "armature": arm.name,
        "bones": len(bones),
        "controls": [o.name for o in controls],
        "arms": [bones[i].name for i in arms],
    }
    print("DRIVER_IK_EXPORT " + json.dumps(report))


if __name__ == "__main__":
    main()
