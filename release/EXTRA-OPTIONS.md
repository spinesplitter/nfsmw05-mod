# Career traffic and opponent speed (gameplay plugin)

F4 → Traffic and HUD includes separate gameplay-plugin controls. They do not
modify Extra Options or its INI:

- **Offline race traffic floor** (`[Traffic] RaceDensityFloor`): default 75,
  range -1 (event default) through 100. Raises the event density request to at
  least this value, including zero-traffic events. Applies when starting the next
  offline race, including Career. This is density, not a car count. Free roam
  uses a separate live density path; this floor alone does not affect free roam.
- **Career traffic multiplier** (`[Traffic] CareerMultiplier`): default 3.0,
  range 1.0 through 3.0. Multiplies the live normalized traffic-density request
  consumed by AISpawnManager in Career races and Career free roam only.
  Above 1.0, free roam uses RaceDensityFloor as its minimum baseline, so the
  default 75 floor requests 2.25 normalized density at 3x in both paths.
  Set 1.0 for the original live density (the separate offline race floor still
  applies). No persistent density mutation or compounding, and no pool/array
  limits are patched. This increases spawn demand, not a guaranteed number of
  visible vehicles. In-game traffic counts and stability remain unverified.
- **11 Career opponents**: the Career Sprint, Circuit and Speedtrap creation
  loop requests 11 AI racers (12 total with the player). Extra slots reuse the
  event's existing valid character profiles through bounded lookups, without
  modifying the original VLT array. No MoreOpponents script or event list is
  imported. Other race types/contexts bypass the hooks. Missing profiles, empty
  rosters, incompatible code or a nonstandard initial racer count retain the
  original roster. Boss races (the game's BossRace flag) and the five opening
  story events (EventID hashes) are excluded in both count and entry hooks,
  preserving their original opponents. The later reversed 16_1_1_r_circuit
  remains eligible. Rewards, laps, catch-up,
  opponent-speed controls and ExOpts are unchanged. The racer-record capacity is
  16; grid, HUD and mission progression with 12 racers still need in-game testing.
- **Career opponent speed** (`[Races] OpponentSpeed`): default 1.0 (100%, stock),
  range 0.25 to 2.0 (25–200%). Applies live to Career racer throttle/brake targets.
  Player, police and civilian traffic controller tables are not modified; other
  race modes bypass it. The target is restored after each controller update, so
  it does not compound. This is not extra engine power, grip or a top-speed guarantee.
  It also scales corner targets: it is not a straight-line-only boost or cornering
  limiter. Career adaptive AI and catch-up logic remain unchanged.
- The gameplay difficulty selector and difficulty-getter hook have been removed.
  Old `[Races] Difficulty` values are ignored. ExOpts and its settings are unchanged;
  ExOpts has no opponent-difficulty setting to move or retain.

Online races bypass both controls. Hooks check executable bytes/vtable slots and log
availability in nfsmw_gameplay.log. Runtime compatibility and visual traffic gains
must still be checked in-game. Do not enable another mod's hooks at these getters.

# Extra Options bundle

This pack aggregates the unmodified **ExOpts Team NFSMW Extra Options v10.0.1.1337**
plugin with our mod. It does not compile ExOpts into our plugin.
Upstream: https://github.com/ExOptsTeam/NFSMWExOpts/releases/tag/v10.0.1.1337
Source revision: `8a094fe29d28195bdcf69122dbfe945f16315f9a`.
The GPLv3 license, upstream readme and corresponding source archive travel with
the binary in `extras/ExtraOptions`. Our bundle changes only its INI profile.

## Install

Extract the pack to the game directory, close the game, then run the included
`powershell -NoProfile -ExecutionPolicy Bypass -File .\Install-ExtraOptions.ps1`
from PowerShell in the extracted game directory. This bypass applies only to
that process, not the machine execution policy. If launching it from elsewhere, pass
`-GameDirectory` with the absolute game directory. It copies only the ExOpts ASI
and, on first installation, its INI into `scripts`; it does **not** replace the
ASI loader or existing settings. Existing ExOpts binaries receive a dated backup.
This explicit install step prevents a release extraction overwriting your ExOpts
configuration. Keep the `extras/ExtraOptions` folder for licensing and source.

## Controls and scope

Open **F4 -> Extra Options** for the core Quick Race limits and traffic presets.
Both normal and classic menus use this page. It edits the installed plugin's INI
beside its loaded ASI, not our gameplay settings. Only changed keys are saved when
F4 closes; **restart the game to apply them**. These are not live controls.
The page shows plugin status and disables editing unless the plugin is loaded and
its INI exists. It does not install or load ExOpts itself. Values are read once per
game session; make external INI edits with the game closed.

The page is a curated settings adapter, not the entire upstream feature set.
Experimental catch-up/memory changes and cheat hotkeys are intentionally not exposed.
For other settings, edit `scripts/NFSMWExtraOptionsSettings.ini` with the game closed:

- `[OpponentControllers] Maximum=15` exposes up to 15 opponents in Quick Race.
  `Minimum=0`, `RandomRace=6`, `MaxUniqueOpponentCars=3` remain upstream defaults.
- `[TrafficControllers] Low=10`, `Medium=30`, `High=100` set Quick Race traffic
  presets. Choose the desired traffic level in the game's race setup.
- All `[Hotkeys]` entries are zero in our profile, reserving F4 and other existing
  bindings. `WorldMapAnywhere` and `SkipTrackAnywhere` are also off.
- Experimental `NoCatchUp=0` and `ExpandMemoryPools=0` remain off. Game speed and
  physics-related settings retain upstream defaults. ExOpts has other patches;
  this is not a traffic-only build and full runtime compatibility is unverified.

The gameplay mod retains the offline opponent-speed controller and racer-only
catch-up toggle. It does not install a traffic-density multiplier or expanded
rosters. ExOpts' NoCatchUp is broader than the gameplay mod's racer-only toggle:
it also patches the police/perp catch-up slot. Normal drag handling/HUD,
heavy-support suppression and the tollbooth starting bonus remain ours.

These menu extensions are **not verified** to increase career/free-roam traffic,
expand active-car pools, or permit stable 15-opponent races on every track.
Test Quick Race first with a backed-up save. Verify F4, wheel/shifter, kerb FFB,
VR mirrors, drag races, pursuits and tollbooth restarts in-game. For rollback,
remove `scripts/NFSMWExtraOptions.asi` with the game closed (or restore its backup).

## Build/package

Run `prepare-extra-options.ps1` to fetch checksum-verified pinned release and
source archives. `package.sh` runs it automatically and stages the plugin under
`extras/ExtraOptions`; the upstream bundled `dinput8.dll` is never included.
The regular package continues to use our chosen Ultimate ASI Loader.