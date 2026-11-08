"""Validate existing view scheduling, then apply a reversible reflection budget."""
import csv
from datetime import datetime
import hashlib
import io
import json
from pathlib import Path
import subprocess
import traceback

root = Path(__file__).resolve().parents[1]
game = Path(r'T:\Games\ModWanted\driverNeed For Speed Most Wanted Black Edition - Copy')
report = root / 'scene-budget-results.json'
result = {'status': 'testing'}
report.write_text(json.dumps(result, indent=2))
try:
    with (root / 'scene-budget-regressions.log').open('w') as log:
        tests = subprocess.run(['cmd.exe', '/d', '/c', 'tests\\optimization_available_tests.cmd'],
                               cwd=root, stdout=log, stderr=subprocess.STDOUT)
    result['regressions_exit_code'] = tests.returncode
    if tests.returncode:
        raise RuntimeError('Regression failure; settings not changed')
    processes = subprocess.run(['tasklist.exe', '/FO', 'CSV', '/NH'],
                               capture_output=True, text=True, check=True)
    if any(row and row[0].lower() in ('speed.exe', 'nfsmw_vr_host.exe')
           for row in csv.reader(io.StringIO(processes.stdout))):
        raise RuntimeError('Close game and VR host before applying settings')
    path = game / 'scripts/nfsmw_vr.ini'
    before = path.read_bytes()
    after = before
    changes = {'EnvMapFacesPerFrame': ('2', '1'), 'RearViewEvery': ('1', '2')}
    for key, (old, new) in changes.items():
        old_line = (key + '=' + old).encode()
        new_line = (key + '=' + new).encode()
        if before.splitlines().count(old_line) != 1:
            raise RuntimeError('Unexpected current setting: ' + key)
        after = after.replace(old_line, new_line, 1)
    assert len(before.splitlines()) == len(after.splitlines())
    assert sum(a != b for a, b in zip(before.splitlines(), after.splitlines())) == 2
    for preserved in (b'KeyTrace=46', b'NearZ=0.091', b'DepthSplit=1', b'SharedViewsOnce=1'):
        assert preserved in after.splitlines(), preserved
    backup = game / 'mod-update-backups' / ('scene-budget-' + datetime.now().strftime('%Y%m%d-%H%M%S-%f'))
    backup.mkdir(parents=True)
    (backup / path.name).write_bytes(before)
    assert (backup / path.name).read_bytes() == before
    try:
        path.write_bytes(after)
        assert path.read_bytes() == after
    except Exception:
        path.write_bytes(before)
        raise
    result.update(status='applied', backup=str(backup / path.name), path=str(path),
                  changes=changes, sha256=hashlib.sha256(after).hexdigest(),
                  only_two_settings_changed=True, live_performance_verified=False)
except Exception:
    result.update(status='failed', error=traceback.format_exc())
    raise
finally:
    report.write_text(json.dumps(result, indent=2))