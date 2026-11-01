# Opponent panels missing on the starting grid

Reported symptom: some opponents, including models different from the player car,
are missing body panels before a race starts.

Extra Options documents its experimental `[Fixes] CarSkinFix` option as a fix for
skinless ghost cars. The installed profile had this option disabled. Enabling it
is a targeted diagnostic/mitigation, not confirmation of the underlying cause.

With the game closed, run `Install-OpponentSkinFix.ps1 -GameDirectory` followed by
the absolute path of the game directory containing `speed.exe`. If local script
execution is disabled, use `powershell.exe -NoProfile -ExecutionPolicy Bypass -File`
followed by the absolute installer path and its `-GameDirectory` argument. This
sets execution policy for that process only, not the system.

`Test-OpponentSkinFix.ps1` tests exact replacement, line endings, backup contents,
idempotence and safe rejection of malformed settings using temporary fixtures.

The installer requires an existing Extra Options INI, verifies a byte-identical
backup, and changes only `CarSkinFix` from `0` to `1` in `[Fixes]`. It preserves
comments, line endings and other settings. Repeating it is a no-op. It does not
install binaries, enable memory-pool expansion, alter traffic or roster hooks,
or change wheel positioning/persistence files.

## Runtime check and rollback

Restart the game and enter a regular Career race previously showing missing
panels. Inspect the starting grid before collisions and check race stability.
Repeat with another event/model if possible. Installation checks cannot establish
that the visual problem is fixed.

If this does not help or introduces instability, close the game and restore the
printed `NFSMWExtraOptionsSettings.ini.before-opponent-skin-fix-*.bak` backup to
`NFSMWExtraOptionsSettings.ini`. Do not overwrite subsequent intentional INI edits
without reviewing them; alternatively set only `[Fixes] CarSkinFix` back to `0`.