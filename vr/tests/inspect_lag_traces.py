import csv
import json
from collections import defaultdict
from pathlib import Path
from statistics import mean

root = Path(r'T:\Games\ModWanted\vr-optimization-install\vr')
game = Path(r'T:\Games\ModWanted\driverNeed For Speed Most Wanted Black Edition - Copy')
lines = []
for number in (21, 22, 23):
    path = game / f'nfsmw_frametrace_{number:03}.csv'
    rows = [{k: float(v) for k, v in r.items()} for r in csv.DictReader(path.open())]
    groups = {'fast': [i for i,r in enumerate(rows) if r['eye0_draws'] and r['frame_ms'] < 16],
              'slow': [i for i,r in enumerate(rows) if r['eye0_draws'] and r['frame_ms'] > 20]}
    lines.append(f'CAPTURE {number}: frames {len(rows)}, groups { {k:len(v) for k,v in groups.items()} }')
    keys = ['frame_ms','headset_wait_ms','eyes_cpu_ms','gpu_frame_ms','update_ms','driver_draw_cpu_ms','driver_pose_cpu_ms','driver_readback_cpu_ms','mirror_draw_cpu_ms','dash_draw_cpu_ms','dash_compose_cpu_ms','other_car_cpu_ms','eye0_draws','eye1_draws','other_world_submits','other_world_triangles','traffic_hint_submits','physics_steps']
    for key in keys:
        lines.append(key + ' ' + ' '.join(f'{g}={mean(rows[i][key] for i in ids):.3f}' for g,ids in groups.items() if ids))
    for start in range(0,30,3):
        subset = [r for r in rows if start <= r['t_s'] < start+3 and r['eye0_draws']]
        if subset:
            lines.append(f't={start}-{start+3}: ' + ' '.join(f'{key}={mean(r[key] for r in subset):.2f}' for key in ['frame_ms','eyes_cpu_ms','headset_wait_ms','other_world_submits']))
    models = defaultdict(lambda:defaultdict(float))
    index_group = {i:g for g,ids in groups.items() for i in ids}
    for r in csv.DictReader(path.with_name(path.stem+'_models.csv').open()):
        g = index_group.get(int(r['frame_index']))
        if g:
            models[r['category']+':'+r['model']][g] += int(r['submits'])
    ranked = sorted(((name, v['fast']/max(1,len(groups['fast'])), v['slow']/max(1,len(groups['slow']))) for name,v in models.items()),key=lambda r:r[2]-r[1],reverse=True)
    lines.append('MODEL SAMPLES (bounded; not complete or per-model timings):')
    lines.extend(f'{name}: fast={fast:.2f} slow={slow:.2f}' for name,fast,slow in ranked[:10])
for path in (root/'src').glob('*'):
    if path.suffix in ('.cpp','.h'):
        text = path.read_text(errors='replace').splitlines()
        for i,line in enumerate(text):
            if any(s in line.upper() for s in ['6DEBBD','6DEC80','6DED37','6DEE66']):
                lines.append(f'{path}:{i+1}\n'+'\n'.join(text[max(0,i-2):i+4]))
(root/'lag-trace-review.txt').write_text('\n'.join(lines))
print('Wrote',root/'lag-trace-review.txt')