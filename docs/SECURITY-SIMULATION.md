# Route-security prototype: simulation results

Version 0.5.0. These are automated simulation results; the new runtime adapter
has not been tested in Skyrim.

## Experiment

The shipped economy and security definitions were run with 32 paired seeds and
seven scenarios. Each seed used a shared 30-day warmup, followed by a change on
day 30 and another 60 simulated days. All configured raider sites started hostile;
the broad Civil War conditions matched the current runtime. There were no player
trades or registered merchants in this experiment.

All scenarios used identical starting stocks, prices, shipments and RNG state
within each seed. Each run saved and restored its economy on day 47. Daily data
and the summary are written by `marco_security_sim`; the figures below compare
pooled daily means for days 31–60 against the unchanged scenario.

| Change | Measurement | Difference |
|---|---|---:|
| Clear Valtheim Towers | Delivered cargo on Whiterun–Eastmarch, both directions | +39.56% |
| Clear Fort Greymoor | Delivered cargo on Whiterun–Reach, both directions | +4.77% |
| Imperial occupation of Greymoor | Delivered cargo on Whiterun–Reach, both directions | +14.87% |
| Clear all nine configured raider sites, including forts | Delivered cargo across the economy | +9.44% |
| Clear all configured raider sites | Falkreath iron price | -1.65% |
| Clear all configured raider sites | Reach silver price | +0.96% |
| Clear Bleak Falls Barrow | Entire final economy state | Exactly unchanged |

Imperial and Stormcloak occupation produced identical economic results when all
other conditions matched. The faction name itself supplies no advantage.

Valtheim's corridor delivered more cargo in all 32 seeds. Clearing all configured
raider sites lowered Falkreath iron prices in all 32 seeds, by 0.98–2.88% in each
seed's first-month mean. Reach silver became more expensive in 30 of 32 seeds;
its pooled increase is not a guarantee for every playthrough.

## What the results mean

Security changes affect shipments substantially, but many sampled prices barely
move under the existing balance. The model's local output, inventory response and
export reserves can absorb the change. Greymoor's added garrison demand did not
meaningfully raise Whiterun food prices in this run; it affected the wider trade
network instead. It would be misleading to promise that every clearing produces
a visible merchant discount, or that a garrison always raises prices locally.

A separate controlled two-market test, with limited imports, used 32 seeds over
90 days. Its final-60-day mean iron price was **14.22% lower** with the road safe.
Adding a garrison raised its destination's mean food price **4.56%**. These isolate
the mechanisms; they are not forecasts for the full Skyrim configuration.

Production and recipe rules were not changed. Normal feedback from stock levels
can still alter output indirectly. Transport capacity is still allocated in the
existing commodity order, which can favor earlier goods. No balance changes were
made merely to amplify the experimental results.

## Coverage and limits

The pilot contains Embershard, Halted Stream, White River Watch, Valtheim,
Red Eagle Redoubt, Karthspire, Greymoor, Sungard and Dunstad. Bleak Falls Barrow is
an explicit crypt exclusion. All ten base-game location IDs and keyword categories
were checked against the installed Skyrim.esm. Route bindings and strengths are
curated experimental values in `security.json`; this is not automatic coverage
of every dungeon, road or mod-added location.

The runtime checks each configured location's tags and cleared state. For the
three forts it additionally reads `CWFort` and `CWOwner`. Vanilla CWScript excludes
initial nonmilitary forts with CWFort = -2 despite already assigning an owner.
CWFort = 0 and positive owner 1/2 indicate military control; negative owner values
are treated as a transfer in progress. Unknown values and mismatched tags are
inactive. This interpretation still needs live checks with the installed load
order. It does not inspect individual soldiers or recognize arbitrary occupation
mods automatically.

The feature introduces no new world records, NPCs, quests, Papyrus scripts or
engine detours. It adds no location-specific rumor text yet; existing price
rumors continue to reflect the simulated markets.

## Automated checks

The security tests cover classification, fort state interpretation, duplicate
observations, same-date scheduling, restoration of hostile control, bounded
combined effects, unconfigured sites, feature disablement, production isolation,
unchanged cargo already in transit, deterministic catch-up, save restoration,
schema-3 migration and malformed-state rollback. Existing economy, trading and
information tests remain in the suite.

Across the full experiment the largest aggregate stock-accounting residual was
3.27e-11 units, within floating-point tolerance.
Crypt clearing preserved the entire final economy exactly in every seed.

## Reproduce

```sh
cmake -S . -B build/security-simulation -G Ninja -DMARCO_BUILD_PLUGIN=OFF -DMARCO_SANITIZERS=OFF -DCMAKE_BUILD_TYPE=Release
cmake --build build/security-simulation -j 6
build/security-simulation/marco_security_tests package/SKSE/Plugins/MarcoPolo/economy.json package/SKSE/Plugins/MarcoPolo/security.json
build/security-simulation/marco_security_sim package/SKSE/Plugins/MarcoPolo/economy.json package/SKSE/Plugins/MarcoPolo/security.json build/security-results 32
```

The output directory must be empty. `daily.csv` contains individual runs;
`summary.json` contains the pooled results. The experiment is deterministic for
the same build, definitions and seeds.

## In-game comparison when installation is wanted

Use a separate test save: this build writes economy schema 4, which older DLLs
cannot load. Existing schemas 1–3 migrate without resetting the economy.

1. Enable debug logging and load a test character. Save once so that the log and
   `MarcoPolo-state.json` capture current site observations.
2. At an uncleared configured camp, confirm its log entry is hostile. Clear it
   normally and check for the cleared observation. Its change becomes effective
   on the next daily simulation tick; prices are not changed instantly.
3. Compare a saved branch where the site remains occupied with a branch where it
   was cleared. Allow several game days for shipments and compare the affected
   route and markets. Retain each branch's matching SKSE co-save.
4. Check Greymoor before and after a real occupation change. An initially assigned
   Civil War owner must not grant a garrison while CWFort remains -2.
5. Save, quit to desktop, reload and confirm site states and pending observations
   are retained. Check ordinary barter totals and the existing rumor popup.

Changing `enabled` to false in `security.json` and restarting disables the feature.
It does not restore an older save schema or undo trades already made.
