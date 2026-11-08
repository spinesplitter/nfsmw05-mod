"""Install the validated mirror-depth module without touching personal settings."""
import csv
from datetime import datetime
import hashlib
import io
import json
from pathlib import Path
import shutil
import subprocess

root = Path(__file__).resolve().parents[1]
game = Path(r"T:\Games\ModWanted\driverNeed For Speed Most Wanted Black Edition - Copy")
results = json.loads((root / "mirror-depth-validation.json").read_text())
assert results == {"source-tests": 0, "regressions": 0, "build": 0}, results
processes = subprocess.run(["tasklist.exe", "/FO", "CSV", "/NH"], capture_output=True, text=True, check=True)
assert not any(row and row[0].lower() in ("speed.exe", "nfsmw_vr_host.exe")
               for row in csv.reader(io.StringIO(processes.stdout))), "Close the game before installing"

def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()

settings = {str(p): digest(p) for p in (game / "scripts").glob("*.ini*") if p.is_file()}
backup = game / "mod-update-backups" / ("mirror-depth-" + datetime.now().strftime("%Y%m%d-%H%M%S-%f"))
backup.mkdir(parents=True)
pairs = [(root / "bin/d3d9.dll", game / "scripts/nfsmw_vr.asi"),
         (root / "bin/nfsmw_vr.pdb", game / "scripts/nfsmw_vr.pdb")]
for source, target in pairs:
    assert source.is_file() and target.is_file(), (source, target)
    shutil.copy2(target, backup / target.name)
installed = []
try:
    for source, target in pairs:
        shutil.copy2(source, target)
        assert digest(source) == digest(target)
        installed.append({"path": str(target), "sha256": digest(target)})
    assert all(digest(Path(p)) == value for p, value in settings.items())
except Exception:
    for _, target in pairs:
        shutil.copy2(backup / target.name, target)
    raise
report = {"backup": str(backup), "installed": installed,
          "settings_unchanged": True, "settings_checked": len(settings), "validation": results}
(backup / "installation.json").write_text(json.dumps(report, indent=2))
(root / "mirror-depth-installation.json").write_text(json.dumps(report, indent=2))
print(json.dumps(report, indent=2))