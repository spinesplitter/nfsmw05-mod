# Offline traffic update

## Confirmed cause and fix

The old per-type-limit hook expected a CALL immediately after the traffic-pattern
hash push at `0x426299`. This executable actually has `89 56 60` (store the
selected index) before the CALL at `0x4262A2`. The old signature therefore rejected
the hook. The installed log contained the post-curve multiplier message but no
per-type-limit message. The corrected signature includes the intervening store.

Disassembly confirms `0x426230` checks accumulated credits against the pattern
rate at +0x0c and applies maximum/percentage constraints at +0x10/+0x14.
The manager adds the curve output times dt to credits at `0x439BF5` onwards.
Scaling those credits alone cannot remove the per-type constraints. Both paths
now use the same bounded multiplier. Private record copies leave the database
unchanged; fixed ten-entry arrays and vehicle pool capacities are NOT expanded.

The public SDK independently documents float mRate and unsigned mMaxInstances /
mPercent fields:
https://github.com/s-b-repo/nfsmw-2005-re/blob/main/docs/nfsplugin_sdk_mw05/Types/Attrib/Layouts/trafficpatternlayout.h

## Settings and drag exclusion

F4's Offline traffic multiplier runs from 100% to 1000% in 50% steps. The existing
`[Traffic] CareerMultiplier` key is retained for saved-setting compatibility.
Default remains 3 (300%). Rate/type limits apply to offline contexts 0/2/3 in
free roam and races. Online context 1 and unsupported contexts are unchanged.
Drag is race type 2 (not the HUD enum); active drag races bypass density floors,
post-curve scaling and per-type limit scaling. Missing active-race parameters
also fail closed. Original event traffic is retained, not forcibly removed.
NormalDragRaces and NormalDragHud controls are unrelated and unchanged.

## Validation and limits

Regression tests cover 1/2/3/5/10x, above-range clamping, offline contexts,
online/unsupported contexts and drag bypass. The installer checks build/test
exit files, backs up the installed gameplay ASI, copies only that ASI, and verifies
its SHA256. User INIs, wheel/VR plugins and game databases are not overwritten.

The new 15-second spawn-attempt/success samples report the original game's result
without bypassing its road, visibility, allocation or vehicle checks. Start with
300%, compare the same route at 100/200/300%, then try 500% if needed. Test free
roam, Circuit/Sprint, drag, restarts and pursuits. A request multiplier is not a
guaranteed visible-car count. A failed pool allocation or unsuitable road can
still limit traffic. Do not expand arbitrary pool/loop bounds to work around it.

Rollback: close the game and restore the gameplay ASI from the backup directory
recorded in `traffic-v2-install.json`. No configuration rollback is necessary.