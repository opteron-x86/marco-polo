# Skyrim True Economy and Merchantry 0.6.0 — test build

Trade routes now divide cargo according to shortages and viable price gaps.
Goods and roads listed first in the configuration no longer take priority.
Export reserves, destination requirements and road capacity are shared across
all orders, with incoming cargo accounted for before dispatch.

The new [merchant experiment](TRANSPORT-SIMULATION.md) tests 32 paired seeds with
60 dealers, finite funds, restocking, repeated player deliveries and save/reload.
The four established trading routes remain profitable. Changes redistribute
supply: some shops gain stock while others face higher prices. Production,
barter settings and security strengths are unchanged.

The route-security pilot still covers nine raider sites and one excluded crypt.
Supported military forts affect transport and add garrison demand. Location and
fort interpretation still need in-game validation. Route bindings and strengths
are configurable in `SKSE/Plugins/MarcoPolo/security.json`; set `enabled` to false
and restart to disable that feature.

Save schema 4 is unchanged from 0.5.0, including cargo already in transit. Schemas
1–3 migrate without replacing trading state; pre-0.5.0 DLLs cannot load schema 4.
Use separate test saves. This update adds no engine hooks, save fields, ESPs or
Papyrus scripts. The optional innkeeper dialogue addon is unchanged.

The Trade Ledger and SKSE Menu Framework pages remain removed. When merging
into a pre-0.4.0 install, remove `Interface/MarcoPoloTradeLedger.swf`; the old
`SKSE/Plugins/MarcoPolo/ledger.json` is ignored. Debug logging exports economy and
information JSON beside the SKSE logs when saving.
