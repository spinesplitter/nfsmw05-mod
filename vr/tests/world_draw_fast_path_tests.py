"""Keep expensive device queries off ordinary world draws; preserve skin gating."""
from pathlib import Path
import re

root = Path(__file__).resolve().parents[1]
source = (root / 'src/d3d9proxy.cpp').read_text()
condition = re.search(
    r'DriverSkinState<IDirect3DDevice9> skinState\(real,\s*(.*?)\);',
    source, re.S).group(1)
terms = [term.strip() for term in condition.split('&&')]
assert terms == ['type == D3DPT_TRIANGLELIST',
                 'Cockpit_IsSkinDraw(s_curIndices,start,count)',
                 'DrawingPlayerView()'], terms

# Exhaustive truth-table equivalence and short-circuit query-count check.
for triangle in (False, True):
    for skin in (False, True):
        for player in (False, True):
            queries = []
            def view():
                queries.append(1)
                return player
            actual = triangle and skin and view()
            assert actual == (triangle and player and skin)
            assert len(queries) == int(triangle and skin)

cockpit = (root / 'src/cockpit.cpp').read_text()
body = cockpit.split('bool Cockpit_IsSkinDraw(', 1)[1].split('\nbool ', 1)[0]
assert 's_armDraws.find(ib)' in body
assert 'draw == s_armDraws.end() || !triangles) return false;' in body
assert 'DrawingPlayerView' not in body
print('PASS: world draws skip view queries; all skin/view gates preserved')