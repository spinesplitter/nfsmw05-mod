"""Run native tests/build and persist subprocess exit codes, not terminal guesses."""
import json
from pathlib import Path
import subprocess
import hashlib
import pefile

ROOT = Path(__file__).resolve().parents[1]
results = {}
exe = ROOT.parent.parent / "driverNeed For Speed Most Wanted Black Edition - Copy" / "speed.exe"
pe = pefile.PE(str(exe))
base = pe.OPTIONAL_HEADER.ImageBase
sites = {}
for site in (0x7291FD, 0x7296C1):
    data = pe.get_data(site-base, 5)
    sites[hex(site)] = data[0] == 0xE8 and site+5+int.from_bytes(data[1:], "little", signed=True) == 0x4FCA70
body = pe.get_data(0x6CF2B0-base, 0x14B)
(ROOT / "stereo-calculations-executable.json").write_text(json.dumps({
    "exe": str(exe), "sha256": hashlib.sha256(exe.read_bytes()).hexdigest(),
    "scenery_calls": sites,
    "bounds_thunk": pe.get_data(0x4FCA70-base,5).hex(),
    "bounds_body_sha256": hashlib.sha256(body).hexdigest(),
    "bounds_body_fnv1a": hex(__import__('functools').reduce(lambda h,b: ((h^b)*16777619)&0xffffffff, body,2166136261)),
},indent=2))
if not all(sites.values()):
    raise SystemExit("unexpected native scenery calls")
for name, command in [
    ("tests", "tests\\stereo_calculations_run.cmd"),
    ("regressions", "tests\\optimization_available_tests.cmd"),
    ("build", "build.cmd"),
]:
    with (ROOT / ("stereo-calculations-" + name + ".log")).open("w") as log:
        result = subprocess.run(["cmd.exe", "/d", "/c", command], cwd=ROOT,
                                stdout=log, stderr=subprocess.STDOUT)
    results[name] = result.returncode
    (ROOT / "stereo-calculations-validation.json").write_text(json.dumps(results, indent=2))
print(json.dumps(results, indent=2))
raise SystemExit(any(results.values()))