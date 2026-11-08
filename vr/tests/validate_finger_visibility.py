"""Persist fresh validation and install only after all checks pass."""
import json
from pathlib import Path
import subprocess
import traceback

root = Path(__file__).resolve().parents[1]
results = {}
report = root / "finger-visibility-validation.json"
report.write_text("{}")
try:
    jobs = [
        ("source-tests", "tests\\finger_visibility_run.cmd"),
        ("regressions", "tests\\optimization_available_tests.cmd"),
        ("build", "build.cmd"),
    ]
    for name, command in jobs:
        with (root / ("finger-visibility-" + name + ".log")).open("w") as log:
            result = subprocess.run(["cmd.exe", "/d", "/c", command], cwd=root,
                                    stdout=log, stderr=subprocess.STDOUT)
        results[name] = result.returncode
        report.write_text(json.dumps(results, indent=2))
        if result.returncode:
            raise RuntimeError(name + " failed; installation skipped")
    # Reuse the guarded, hash-verified installer with this workflow's fresh
    # validation/report/backup names. Its game-process and INI checks stay intact.
    installer = (root / "tests/install_mirror_depth.py").read_text()
    installer = installer.replace("mirror-depth", "finger-visibility")
    with (root / "finger-visibility-install.log").open("w") as log:
        script = "__file__ = " + repr(str(root / "tests/install_mirror_depth.py")) + "\n" + installer
        result = subprocess.run(["python", "-c", script], cwd=root,
                                stdout=log, stderr=subprocess.STDOUT)
    if result.returncode:
        raise RuntimeError("Installation failed; see finger-visibility-install.log")
except Exception:
    (root / "finger-visibility-error.log").write_text(traceback.format_exc())
    raise