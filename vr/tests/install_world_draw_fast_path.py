"""Validate, build and install the world-draw fast path with rollback on failure."""
import json
from pathlib import Path
import subprocess

root = Path(__file__).resolve().parents[1]
results = {}
report = root / 'world-draw-validation.json'
report.write_text('{}')
for name, command in [
    ('source-tests', ['python', str(root / 'tests/world_draw_fast_path_tests.py')]),
    ('preservation-tests', ['cmd.exe', '/d', '/c', 'tests\\finger_visibility_run.cmd']),
    ('regressions', ['cmd.exe', '/d', '/c', 'tests\\optimization_available_tests.cmd']),
    ('build', ['cmd.exe', '/d', '/c', 'build.cmd']),
]:
    with (root / ('world-draw-' + name + '.log')).open('w') as log:
        result = subprocess.run(command, cwd=root, stdout=log, stderr=subprocess.STDOUT)
    results[name] = result.returncode
    report.write_text(json.dumps(results, indent=2))
    if result.returncode:
        raise RuntimeError(name + ' failed; installation skipped')

# Reuse the existing process guard, timestamped backups, hash checks, INI
# preservation and rollback rather than introducing another installation path.
installer_path = root / 'tests/install_mirror_depth.py'
installer = installer_path.read_text().replace('mirror-depth', 'world-draw')
installer = installer.replace(
    '{"source-tests": 0, "regressions": 0, "build": 0}',
    '{"source-tests": 0, "preservation-tests": 0, "regressions": 0, "build": 0}')
with (root / 'world-draw-install.log').open('w') as log:
    result = subprocess.run(
        ['python', '-c', '__file__ = ' + repr(str(installer_path)) + '\n' + installer],
        cwd=root, stdout=log, stderr=subprocess.STDOUT)
if result.returncode:
    raise RuntimeError('Installation failed; see world-draw-install.log')
print((root / 'world-draw-installation.json').read_text())