"""Guard stock traffic behavior and the racer-only catch-up setting."""
from pathlib import Path
import re

root = Path(__file__).resolve().parents[1]
source = '\n'.join(p.read_text() for p in (root / 'src').glob('*') if p.suffix in {'.h', '.cpp'})
for removed in ('extra_options', 'ExtraOptions', 'ExOpts', 'ExtraStatus', 'RaceDensityFloor', 'CareerMultiplier',
                'InstallCareerRoster', 'InstallRaceOverrides', 'CareerSpawnCount',
                'CareerSpawnEntry', 'TrafficSpawnAttempt', 'TrafficPatternEntry'):
    assert removed not in source, f'Removed integration returned: {removed}'
traffic = (root / 'src/traffic.cpp').read_text()
for address in ('0x439A39', '0x439A44', '0x439A75', '0x439C6B', '0x4262A2',
                '0x439BBD', '0x439BF0', '0x5FABB0', '0x61DCC8', '0x61DCB7'):
    assert address not in traffic, f'Traffic/roster patch returned: {address}'
package = (root.parent / 'package.sh').read_text()
assert not re.search('extra-options|ExtraOptions', package)
ui = (root / 'src/ui.cpp').read_text()
traffic_page = ui.split('PAGE_("TRAFFIC AND HUD"),', 1)[1].split('PAGE_(', 1)[0]
assert traffic_page.index('"TollboothTimeBonus"') < traffic_page.index('"OpponentSpeed"')
assert ui.count('"OpponentSpeed", &g_cfg.opponentSpeed') == 1
assert '{&g_cfg.opponentSpeed, "Offline race opponents only;' in ui
assert '"DisableRubberband", &g_cfg.disableRubberband' in traffic_page
assert 'SetSlot(0x8925C8, 0x409390, (void*)&RacerCatchup)' in traffic
assert 'if (g_cfg.disableRubberband) return 0.0f;' in traffic
assert 'police catch-up unchanged' in traffic
print('PASS: offline race opponent-speed slider and help remain in Traffic and HUD')
print('PASS: racer-only catch-up toggle is restored without changing police behavior')
print('PASS: no traffic increases, expanded rosters, ExtraOptions adapter or package integration')