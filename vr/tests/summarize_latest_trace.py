import csv
import json
from pathlib import Path
from statistics import mean

game = Path(r'T:\Games\ModWanted\driverNeed For Speed Most Wanted Black Edition - Copy')
path = max((p for p in game.glob('nfsmw_frametrace_*.csv') if p.stem.split('_')[-1].isdigit()), key=lambda p:p.stat().st_mtime)
rows = [{k:float(v) for k,v in row.items()} for row in csv.DictReader(path.open())]
active = [row for row in rows if row['eye0_draws'] > 0 and row['eye1_draws'] > 0]
def stats(values):
    values = sorted(values)
    return {'mean':round(mean(values),4), 'p95':round(values[int((len(values)-1)*.95)],4), 'max':round(values[-1],4)} if values else None
fields = [k for k in rows[0] if k.endswith('_ms')]
groups = {'active':active, 'fast_under16ms':[r for r in active if r['frame_ms']<16], 'slow_over20ms':[r for r in active if r['frame_ms']>20]}
report = {'capture':str(path), 'modified':path.stat().st_mtime, 'rows':len(rows), 'duration_s':rows[-1]['t_s']-rows[0]['t_s'], 'groups':{}}
for name, group in groups.items():
    report['groups'][name] = {'frames':len(group), 'timings':{k:stats([r[k] for r in group if r[k]>=0]) for k in fields}}
(Path(__file__).resolve().parents[1]/'latest-trace-summary.json').write_text(json.dumps(report,indent=2))
print(json.dumps(report,indent=2))