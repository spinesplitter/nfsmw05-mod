import csv
import importlib.util
import tempfile
from pathlib import Path

spec = importlib.util.spec_from_file_location('analysis', Path(__file__).with_name('analyze-pass-trace.py'))
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)
with tempfile.TemporaryDirectory() as tmp:
    p = Path(tmp) / 'capture.csv'
    p.write_text('frame_ms,eye0_draws,eye0_cpu_ms,eye1_cpu_ms\n12,10,2,1\n24,10,5,1\n')
    spans = p.with_name('capture_passes.csv')
    spans.write_text('frame_index,eye,from_site,to_site,cpu_ms,calls,overflow\n'
                     '0,0,00000000,006DE749,0,1,0\n0,0,006DE749,806DE749,2,1,0\n'
                     '0,0,806DE749,FFFFFFFF,0,1,0\n0,1,00000000,FFFFFFFF,1,1,0\n'
                     '1,0,00000000,006DE749,0,1,0\n1,0,006DE749,806DE749,5,1,0\n'
                     '1,0,806DE749,FFFFFFFF,0,1,0\n1,1,00000000,FFFFFFFF,1,1,0\n')
    report = module.analyze(p)
    assert report['intervals'][0]['increase_ms'] == 3
    assert report['max_partition_vs_eye_error_ms'] == 0
    assert report['intervals'][0]['kind'] == 'queued_render_routine'
    good = spans.read_text()
    for bad in (good.replace('0,0,806DE749,FFFFFFFF,0,1,0\n', ''),
                good.replace('806DE749,2,1,0', '806DE749,8,1,0'),
                good.replace('806DE749,2,1,0', '806DE749,nan,1,0'),
                good.replace('0,1,00000000,FFFFFFFF,1,1,0\n', '')):
        spans.write_text(bad)
        try:
            module.analyze(p)
            raise AssertionError('invalid or incomplete partition accepted')
        except ValueError:
            pass
    spans.write_text(good)
    spans.write_text(spans.read_text().replace(',1,0\n', ',1,1\n'))
    try:
        module.analyze(p)
        raise AssertionError('overflow accepted')
    except ValueError:
        pass
    spans.unlink()
    try:
        module.analyze(p)
        raise AssertionError('missing timings accepted')
    except ValueError:
        pass
print('Pass analyzer tests passed: ranking, conservation, overflow rejection, legacy rejection')