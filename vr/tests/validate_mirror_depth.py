"""Run mirror-depth checks and the normal VR build, persisting exit codes."""
import json
from pathlib import Path
import subprocess
import sys

root = Path(__file__).resolve().parents[1]
results = {}
jobs = [
    ("source-tests", [sys.executable, str(root / "tests/mirror_depth_source_tests.py")]),
    ("regressions", ["cmd.exe", "/d", "/c", str(root / "tests/optimization_available_tests.cmd")]),
    ("build", ["cmd.exe", "/d", "/c", str(root / "build.cmd")]),
]
for name, command in jobs:
    with (root / ("mirror-depth-" + name + ".log")).open("w") as log:
        result = subprocess.run(command, cwd=root, stdout=log, stderr=subprocess.STDOUT)
    results[name] = result.returncode
    (root / "mirror-depth-validation.json").write_text(json.dumps(results, indent=2))