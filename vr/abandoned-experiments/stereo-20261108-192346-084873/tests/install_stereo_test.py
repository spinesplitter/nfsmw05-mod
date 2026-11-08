"""Install the validated stereo test build without replacing player settings."""
import ctypes
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
from datetime import datetime

root = Path(__file__).resolve().parents[1]
game = root.parent.parent / 'driverNeed For Speed Most Wanted Black Edition - Copy'
state = json.loads((root / 'stereo-calculations-validation.json').read_text())
assert all(state.get(k) == 0 for k in ('tests', 'regressions', 'build')), state
processes = subprocess.check_output(['tasklist', '/fo', 'csv', '/nh']).decode(errors='replace').lower()
assert not any(('"' + name + '"') in processes for name in ('speed.exe', 'nfsmw_vr_host.exe')), 'Close game/VR host first'
assert not (game / 'nfsmw_vr.ini').exists(), 'Root INI overrides scripts INI; inspect before installing'
ini = game / 'scripts' / 'nfsmw_vr.ini'
assert ini.is_file()
pairs = [(root / 'bin' / 'd3d9.dll', game / 'scripts' / 'nfsmw_vr.asi'),
         (root / 'bin' / 'nfsmw_vr_host.exe', game / 'nfsmw_vr_host.exe'),
         (root / 'bin' / 'nfsmw_crashdump.exe', game / 'nfsmw_crashdump.exe')]
assert all(src.is_file() and dst.is_file() for src, dst in pairs)
backup = game / 'mod-update-backups' / ('stereo-test-' + datetime.now().strftime('%Y%m%d-%H%M%S-%f'))
backup.mkdir(parents=True)
for dst in [ini] + [dst for _, dst in pairs]:
    shutil.copy2(dst, backup / dst.name)
profile = ctypes.WinDLL('kernel32', use_last_error=True)
write = profile.WritePrivateProfileStringW
write.argtypes = [ctypes.c_wchar_p] * 4
write.restype = ctypes.c_int
read = profile.GetPrivateProfileStringW
read.argtypes = [ctypes.c_wchar_p, ctypes.c_wchar_p, ctypes.c_wchar_p, ctypes.c_wchar_p, ctypes.c_uint, ctypes.c_wchar_p]
def setting(key):
    buf = ctypes.create_unicode_buffer(256)
    read('VR', key, '', buf, len(buf), str(ini))
    return buf.value
preserved = {key: setting(key) for key in ('NearZ', 'DepthSplit', 'Enabled')}
try:
    for src, dst in pairs:
        shutil.copy2(src, dst)
        assert hashlib.sha256(src.read_bytes()).digest() == hashlib.sha256(dst.read_bytes()).digest()
    for key in ('SharedStereoCalculations', 'VerifyStereoCalculations'):
        assert write('VR', key, '1', str(ini)), ctypes.get_last_error()
        assert setting(key) == '1'
    assert all(setting(k) == v for k, v in preserved.items())
except BaseException:
    for dst in [ini] + [dst for _, dst in pairs]:
        shutil.copy2(backup / dst.name, dst)
    raise
result = {'backup': str(backup), 'ini': str(ini), 'settings': {**preserved, 'SharedStereoCalculations': setting('SharedStereoCalculations'), 'VerifyStereoCalculations': setting('VerifyStereoCalculations')},
          'installed': {str(dst): hashlib.sha256(dst.read_bytes()).hexdigest() for _, dst in pairs}}
(root / 'stereo-test-installation.json').write_text(json.dumps(result, indent=2))
print(json.dumps(result, indent=2))