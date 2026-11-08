"""Build/test the idle boundary fast path and reuse the guarded ASI installer."""
import json
from pathlib import Path
import subprocess
import traceback

root = Path(__file__).resolve().parents[1]
results = {}
report = root / 'boundary-idle-validation.json'
report.write_text('{}')
try:
    for name, command in [('source-tests', 'tests\\finger_visibility_run.cmd'),
                          ('regressions', 'tests\\optimization_available_tests.cmd'),
                          ('build', 'build.cmd')]:
        with (root / ('boundary-idle-' + name + '.log')).open('w') as log:
            result = subprocess.run(['cmd.exe', '/d', '/c', command], cwd=root,
                                    stdout=log, stderr=subprocess.STDOUT)
        results[name] = result.returncode
        report.write_text(json.dumps(results, indent=2))
        if result.returncode:
            raise RuntimeError(name + ' failed; installation skipped')
    installer_path = root / 'tests/install_mirror_depth.py'
    installer = installer_path.read_text().replace('mirror-depth', 'boundary-idle')
    with (root / 'boundary-idle-install.log').open('w') as log:
        result = subprocess.run(['python', '-c', '__file__ = ' + repr(str(installer_path)) + '\n' + installer],
                                cwd=root, stdout=log, stderr=subprocess.STDOUT)
    if result.returncode:
        raise RuntimeError('Installation failed; see boundary-idle-install.log')
except Exception:
    (root / 'boundary-idle-error.log').write_text(traceback.format_exc())
    raise