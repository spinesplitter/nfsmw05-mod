# Solid-cache overhead reduction

The draw-path identity check uses a non-owning `SolidIdentity`, avoiding construction
of owning containers on cache hits. Solid names are canonicalized after their null
terminator, so unrelated trailing bytes do not force cache rebuilds. New entries
are moved into the cache rather than deep-copied.

Ordinary traffic, scenery and breakable meshes no longer have their full index
payload hashed on a cache miss: they never use that baseline. Driver and wheel
reload checks remain enabled; mirror splits establish their own index hash.

Run `vr/tests/solid_cache_run.cmd` for the production cache regression, including
10,000 allocation-free hits and protected index memory on misses, hits and address
reuse. Run `sh gameplay/tests/run.sh` for the per-car/group and profile export tests.

This branch descends from upstream gameplay commit
`b2762ee1aa1ae5b34801dc46251a6c2eda1c9387`: car character, tyre peak angle,
wheelspin in slides and throttle/side grip are per-car/group profile values, and
downforce uses the squared-speed shape when physics sliders are enabled.

No resolution, stereo, visual, physics tuning, FFB or user INI changes are part of
the cache optimization. It targets CPU overhead and streaming spikes, not a proven
72 FPS fix. Traffic/breakable-related headset framedrops remain unresolved.