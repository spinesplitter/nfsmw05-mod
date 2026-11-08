# Stock traffic restoration

Traffic density floors, spawn-rate multipliers, per-type traffic-limit scaling,
spawn diagnostic hooks and forced 11-opponent Career rosters have been removed.
The gameplay module no longer reads or writes ExtraOptions settings, and the
package script no longer bundles ExtraOptions or its installer.

Old `Traffic.RaceDensityFloor`, `Traffic.CareerMultiplier` and `Traffic.Multiplier` keys are ignored
by the rebuilt plugin. The local restoration installer also removes them from
existing settings, along with obsolete `Races.DisableRubberband`; no catch-up override is installed.
Unrelated opponent-speed, pursuit, drag driving/HUD, tollbooth bonus, physics,
wheel, VR and cockpit preferences are retained.

The local restoration installer (not distributed in this source update) backs up the installed modules and affected settings,
disables ExtraOptions by renaming its ASI, and restores its six adapter-managed
keys to documented upstream defaults (opponents 15/6, laps 127, traffic 10/30/100).
These are ExtraOptions defaults, not stock game menu limits. Disabling its ASI
restores the game's own runtime limits and behavior on the next launch.

For a manual update, back up the installation, close the game, and disable any
previously installed ExtraOptions ASI if stock traffic behavior is wanted.
Updating this source alone does not remove an independently installed plugin.

No game database or save file is rewritten: the removed hooks were runtime-only
and used private traffic-pattern copies. This does not certify that unrelated
third-party database mods or already-saved Quick Race selections are stock.