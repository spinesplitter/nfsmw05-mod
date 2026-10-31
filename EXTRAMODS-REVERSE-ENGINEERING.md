# Extra mods: static script analysis

## Scope and limits

Inspected both original .nfsms files in the game's extramods directory. These are plaintext VltEd-style database import scripts, not ASI/DLL machine-code patches. No imports, gameplay-source edits, installations, or game launches were performed. Counts below describe explicit script operations assuming successful sequential import; the installed database and runtime behavior were not verified. copy_fields, inheritance, existing database contents, and importer errors may affect actual results.

## IncreaseTrafficMW.nfsms

- 41 nonblank update_field commands.
- Sets Vehicles[i].Density to 10 for 39 entries across seven trafficpattern records:
  cityhighway (6), coastalnorth (5), coastalsouth (6), collegehighway (6),
  collegenorth (5), collegesouth (5), downtown (6).
- Sets trafficpattern/default SpawnTime to 0 twice (same assignment repeated).
- Does not modify race TrafficLevel, executable instructions, vehicle pools,
  MaxInstances, opponent rosters, or physics.
- Density=10 is a database field value, not evidence of ten active vehicles or
  a tenfold increase. SpawnTime=0 removes the configured delay if its consumer
  treats this as a delay; exact spawn scheduling and inheritance need runtime
  or consumer verification.

The existing gameplay/src/traffic.cpp instead scales the normalized return from
0x41EC20, clamps it to [0,1], and preserves zero. Therefore this script changes
a separate traffic input layer; it does not prove a global traffic-cap bypass.
Our multiplier cannot exceed a saturated normalized density of 1.0.

## More opponents in career races.nfsms

1943 commands: 1680 update_field, 201 resize_field, 45 add_field,
10 copy_fields, 7 delete_field.

Mechanism: resize existing gameplay/Opponents arrays, or add them to events such
as tollbooths, then populate entries with existing character/opponent references.
References are repeated; this does not define a new unique character for every
slot. The first three existing entries are often left intact.

Sequential replay of explicit add_field/resize_field roster sizes yields:

| Opponents | Explicitly targeted events |
| ---: | ---: |
| 1 | 1 |
| 3 | 3 |
| 5 | 17 |
| 7 | 15 |
| 9 | 14 |
| 11 | 94 |
| 15 | 2 |
| Total | 146 |

These are roster lengths, not measured successful spawns. Normally the player
is additional: 11 opponents means a nominal 12-car field. The two final explicit
15-slot targets are 16_1_1_circuit and race_bin_14/14_3_1_lapknockout.
Late resizes reduce many earlier 15-slot arrays to 11; reading only the opening
lines gives the wrong result. No explicit out-of-range opponent writes relative
to known preceding sizes were found. Temporary null assignments are overwritten;
none remain in the replayed explicitly assigned roster entries. Existing entries
and references were not resolved against the installed database.

### Bundled changes beyond opponents

- TrafficLevel writes on 142 distinct records; final explicit writes are 100
  except race_bin_12/12_7_3_drag, which is 0.
- NumLaps writes on 22 records end at 10, overriding earlier 15-lap values.
- CashValue changes on 23 records, with final explicit values up to 1,000,000.
- Pursuit/heat settings, one CatchUpSkill=10 write, and KnockoutsPerLap=2.
- Rolling-start and intro changes; one Name assignment.
- TimeLimit=100 for race_bin_05/7_4_2_r_tollbooth.
- Ten optional-field copy operations among tollbooth records; deletion of
  Shortcuts and two BarrierExemptions fields, plus other field deletions.

Scalar-write statistics are not complete database emulation: copy_fields can
also transfer optional fields, and later deletions can remove written values.

## Implications for the current mod

- Opponent count is event roster data, separate from opponent speed/catch-up.
- Traffic has at least separate race/live-density and trafficpattern inputs.
- Increasing our live multiplier alone cannot reproduce these pattern edits.
- Importing the opponent script wholesale would also change race balance,
  payouts, laps, traffic, pursuits, and tollbooth behavior.
- A narrow implementation should separate roster size, pattern density, spawn
  timing, and existing live density controls; preserve zero/disabled requests.
- Neither script establishes safe engine capacity or allocates larger pools.
  Large fields, especially drag/tollbooth events, need transition, grid, HUD,
  performance, and stability tests before recommending deployment.

Supporting generated artifacts: extramods-inventory.txt,
extramods-opponents-analysis.json, extramods-summary.txt,
extramods-details.txt, extramods-validated-counts.txt.