"""Package the fitted Gallardo overlay; never edit the source rig or game geometry."""
import argparse
import hashlib
import json
from pathlib import Path
import struct
import sys
from PIL import Image

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--preview', required=True, type=Path)
    parser.add_argument('--car', help='AC car folder; otherwise retain the last exported car')
    args = parser.parse_args()
    sys.path.insert(0, str(args.preview))
    import steering_debug as debug
    from driver_rig import GeneratedDriverRig
    root = Path(__file__).resolve().parents[3]
    folder = root / 'drivermod'
    rigfile = args.preview / 'assets/gallardo_driver.rig.json'
    digest = hashlib.sha256(rigfile.read_bytes()).hexdigest()
    joints = {k: tuple(v) for k, v in json.loads(rigfile.read_text())['joints'].items()}
    model = debug.load_obj(args.preview / 'assets/gallardo_driver.obj')
    rig = GeneratedDriverRig(model, joints)
    manifest_path = folder / 'driver-overlay-manifest.json'
    previous = json.loads(manifest_path.read_text()) if manifest_path.exists() else {}
    car = args.car or previous.get('source_car') or debug.DEFAULT_CAR
    if Path(car).name != car or '/' in car or '\\' in car or car in ('.', '..'):
        raise ValueError('Expected an AC car folder name')
    clip = debug.ac_steering.find_content_root() / 'cars' / car / 'animations/steer.ksanim'
    animation = debug.load_animation(clip).ac_model
    if animation is None:
        raise ValueError('Source steering trajectories are required')
    anchor = (-0.11, 0.85, -0.401677)
    def game(p):
        return (p[0]-anchor[0], -(p[2]-anchor[2]), p[1]-anchor[1])
    def offset(p):
        return (p[0], -p[2], p[1])
    vertices, weights = [], []
    # Cut below the saved neck joint, removing neck/chin as well as the head.
    neck_cut = joints['neck'][1] - 0.015
    arm_bones = {i for i,b in enumerate(rig.bones)
                 if any(name in b.name for name in ('UpperArm','Forearm','Hand'))}
    preserved_arm_faces = 0
    for part in model.parts:
        for face, uv in zip(part.faces, part.texture_faces):
            polygon = [(model.vertices[i], uv[j], {i: 1.0}) for j, i in enumerate(face)]
            clipped = []
            # A global height cut also slices the raised fingers. Only exempt
            # faces whose three vertices are predominantly weighted to arms.
            arm_face = all(sum(w.weight for w in rig.weights[i] if w.bone in arm_bones) > 0.5
                           for i in face)
            if arm_face:
                clipped = polygon
                preserved_arm_faces += 1
            for a, b in zip(polygon, polygon[1:]+polygon[:1]):
                if arm_face:
                    break
                if a[0][1] <= neck_cut:
                    clipped.append(a)
                if (a[0][1] <= neck_cut) != (b[0][1] <= neck_cut):
                    t = (neck_cut-a[0][1])/(b[0][1]-a[0][1])
                    p = tuple(x+(y-x)*t for x,y in zip(a[0],b[0]))
                    tex = tuple(x+(y-x)*t for x,y in zip(a[1],b[1]))
                    blend = {i: w*(1-t) for i,w in a[2].items()}
                    for i,w in b[2].items():
                        blend[i] = blend.get(i,0)+w*t
                    clipped.append((p,tex,blend))
            for j in range(1,len(clipped)-1):
                for p,tex,blend in (clipped[0],clipped[j],clipped[j+1]):
                    vertices.append((*game(p),tex[0],1-tex[1]))
                    row = {}
                    for i, fraction in blend.items():
                        for influence in rig.weights[i]:
                            row[influence.bone] = row.get(influence.bone,0)+fraction*influence.weight
                    weights.append([(b,w,offset(tuple(p[k]-rig.bones[b].head[k] for k in range(3)))) for b,w in row.items()])
    assert vertices and preserved_arm_faces > 0
    texture = folder / 'DRIVER-PLAYER.png'
    if not texture.exists():
        texture = folder / 'DRIVER_PLAYER.png'
    image = Image.open(texture).convert('RGBA')
    with (folder / 'driver-runtime.bin').open('wb') as f:
        f.write(struct.pack('<4sIII',b'CDR1',len(vertices),*image.size))
        for v in vertices:
            f.write(struct.pack('<5f',*v))
        f.write(image.tobytes('raw','BGRA'))
    samples = 401
    with (folder / 'driver-animation.bin').open('wb') as f:
        f.write(struct.pack('<4sIII',b'DRA2',len(rig.bones),len(vertices),samples))
        arms = [rig.bone_indices[f'Bip01 {side} {name}'] for side in ('L','R') for name in ('UpperArm','Forearm','Hand')]
        f.write(struct.pack('<6I',*arms))
        for bone in rig.bones:
            f.write(struct.pack('<6f',*game(bone.head),*game(bone.tail)))
        # Canonical wheel frame: X column, Y left, Z up. The preview has Y-up.
        for i in range(samples):
            targets = animation.steering_hand_targets(i/(samples-1),(0,0,0),1,-3.141592653589793/2)
            for side in ('L','R'):
                f.write(struct.pack('<3f',*offset(targets[side])))
        for row in weights:
            f.write(struct.pack('<I',len(row)))
            for bone,w,p in row:
                f.write(struct.pack('<If3f',bone,w,*p))
    assert hashlib.sha256(rigfile.read_bytes()).hexdigest() == digest
    manifest = {'source_rig_sha256':digest,'source_model':str(args.preview / 'assets/gallardo_driver.obj'),
                'source_car':car,'source_animation':str(clip),
                'source_animation_sha256':hashlib.sha256(clip.read_bytes()).hexdigest(),
                'texture':str(texture),'vertices':len(vertices),'trajectory_samples':samples,
                'mapping':'experimental +450 to -450 degrees, no NIS','anchor':anchor,
                'neck_cut_model_y':neck_cut,'texture_addressing':'wrap',
                'neck_cut_excludes_arms':True,'preserved_arm_faces':preserved_arm_faces}
    (folder / 'driver-overlay-manifest.json').write_text(json.dumps(manifest,indent=2))
    print(json.dumps(manifest,indent=2))

if __name__ == '__main__':
    main()