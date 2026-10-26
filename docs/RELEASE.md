# Skyrim True Economy and Merchantry 0.5.0 — test build

Selected bandit and Forsworn locations now affect the capacity, delays and loss
risk of associated trade routes. Clearing them removes their contribution on
subsequent daily ticks. Military occupation of supported forts reduces transport
risk and adds demand for food, grain, medicine, weapons and armor.

The pilot covers ten configured locations, including one draugr crypt that has
no economic effect. Production rules are unchanged. See the
[simulation results and test instructions](SECURITY-SIMULATION.md).

The DLL reads location state without changing quests, ownership, NPCs or world
objects. Fort interpretation follows vanilla Civil War participation and owner
data; it still needs in-game validation. Route assignments and strengths are
configurable in `SKSE/Plugins/MarcoPolo/security.json`. Set `enabled` to `false`
in that file to disable this feature, then restart Skyrim.

Economy save schema 4 adds site states and pending dated observations. Schemas
1–3 migrate without replacing existing stocks, prices, merchants or shipments.
Older DLLs cannot load schema 4, so use separate test saves when evaluating this
build. Rumor history keeps its existing format. No new ESP or Papyrus script is
required; the optional innkeeper dialogue addon is unchanged.

The Trade Ledger and SKSE Menu Framework pages remain removed. When merging
into a pre-0.4.0 install, remove `Interface/MarcoPoloTradeLedger.swf`; the old
`SKSE/Plugins/MarcoPolo/ledger.json` is ignored. Debug logging exports economy and
information JSON beside the SKSE logs when saving.
