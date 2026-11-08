"""Archive the abandoned stereo experiment, preserving unrelated hand/depth fixes."""
from datetime import datetime
from pathlib import Path
import shutil

root = Path(__file__).resolve().parents[1]
archive = root / 'abandoned-experiments' / ('stereo-' + datetime.now().strftime('%Y%m%d-%H%M%S-%f'))
archive.mkdir(parents=True)
for name in ('game.cpp', 'common.cpp', 'common.h'):
    shutil.copy2(root / 'src' / name, archive / name)

def remove_between(text, start, end):
    assert text.count(start) == 1, start
    first = text.index(start)
    last = text.index(end, first)
    return text[:first] + text[last:]

path = root / 'src/game.cpp'
text = path.read_text()
text = text.replace('#include "stereo_native_calculations.h"\n', '').replace('#include <xmmintrin.h>\n', '')
text = remove_between(text, '// Render-thread-local and scoped to a single stereo frame.', '// With the depth split on this frame,')
text = text.replace('return SharedMatrixMultiply(out, local, viewProj);', 'return ((MatrixMultiply_t)kMatrixMultiply)(out, local, viewProj);')
text = text.replace('return SharedMatrixMultiply(out, local, view);', 'return ((MatrixMultiply_t)kMatrixMultiply)(out, local, view);')
text = text.replace('    s_calculationScope=false;\n', '')
text = text.replace('    s_stereoCalculations.End();\n', '')
text = text.replace('    s_stereoCalculations.Begin(g_cfg.sharedStereoCalculations && s_nativeBoundsHookOk,\n                              g_cfg.verifyStereoCalculations);\n', '')
text = text.replace('        s_calculationScope=g_cfg.sharedStereoCalculations && s_nativeBoundsHookOk && !s_stereoCalculations.Failed();\n', '')
# Removing an indented line above must not leave its leading four spaces behind.
text = text.replace('        Trace_RenderEnd();', '        Trace_RenderEnd();')
text = remove_between(text, '    if(g_cfg.sharedStereoCalculations) {', '    s_seatCameraSet = false;')
text = remove_between(text, '    // All-or-nothing validation; never overwrite another mod\'s scenery hook.', '    EnlargeFrameMemory();')
assert not any(word in text for word in ('s_stereoCalculations', 's_calculationScope', 'SharedMatrixMultiply', 'hkSceneryBounds', 'sharedStereoCalculations', 's_nativeBoundsHookOk'))
path.write_text(text)
for name in ('common.cpp', 'common.h'):
    path = root / 'src' / name
    lines = path.read_text().splitlines(keepends=True)
    path.write_text(''.join(line for line in lines if 'sharedStereoCalculations' not in line and 'verifyStereoCalculations' not in line))

files = [root / 'src' / name for name in ('stereo_calculation_cache.h', 'stereo_native_calculations.h', 'stereo_visibility.h')]
files += [root / 'tests' / name for name in ('stereo_calculations_run.cmd', 'stereo_calculations_tests.cpp', 'stereo_visibility_run.cmd', 'stereo_visibility_tests.cpp', 'validate_stereo_calculations.py', 'install_stereo_test.py')]
files += [root / 'STEREO-VISIBILITY.md']
for path in files:
    if path.exists():
        target = archive / path.relative_to(root)
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.move(str(path), str(target))
(root / 'stereo-removal-archive.txt').write_text(str(archive))
print('Archived abandoned stereo code at', archive)