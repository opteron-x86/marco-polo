# Marco Polo 0.2.1 — Innkeeper dialogue correction

The 0.2.0 Trade Rumors addon loaded and its quest started in the reported save,
but the innkeeper option did not appear. This build replaces its empty response
with a short unvoiced line and writes the dialogue groups in canonical order.
Form IDs, conditions, quest identity, scripts and saved information stay the
same. The correction needs an in-game dialogue check; the exact cause has not
yet been established.

With debug logging enabled, opening dialogue now records the speaker, innkeeper
eligibility, availability global, quest state, INFO conditions and record links.
These checks only report state; they do not restart quests or alter conditions.

Papyrus source files now retain LF line endings on Windows checkouts. This fixes
the 0.2.0 CI packaging failure caused by source hashes changing during checkout;
the source/bytecode manifest remains enforced.

## Remembered markets and trade rumors

Trade Status replaces the normal live market ledger. Visiting a town or inn,
opening barter and completing trades record local commodity rates. Distant
markets retain their last known values, with dates, sources and stale-report
labels. Unknown markets remain unknown. Possible trade routes compare only the
reports the player has gathered.

The optional Trade Rumors addon adds an unvoiced **Any news of trade?** option to
innkeepers. Its popup records a few market reports in the tracker. Distant news
comes from actual saved market history and is delayed by trade-route travel time;
early in a new economy, innkeepers discuss local conditions. Each hold shares
one saved daily batch. Asking again or reloading does not reroll the reports.

Remembered reports and bounded news history have their own co-save record.
Existing trading saves retain their economies and begin gathering information
through play. Trading prices, supply behavior, merchant gold and service costs
are unchanged. The former live ledger remains available as **Live markets
(debug)** with debug logging enabled.

Install the main archive, plus the matching `-rumors.zip` for innkeeper dialogue.
Enable `MarcoPolo - Trade Rumors.esp`; it is ESL flagged and has only Skyrim.esm as
a master. The core remains DLL-only. SKSE Menu Framework 3 supplies Trade Status;
the rumor popup also works without that framework. No Missives dependency,
inventory notes, voiced dialogue or silence-mod dependency is introduced.

Targets desktop SE 1.5.97 and AE 1.6.x / 1.7.x through alandtse CommonLibSSE-NG.
Automated tests cover information isolation, dated news, save restoration,
anti-reroll behavior, bounded history, unchanged simulation results and dialogue
record structure. In-game dialogue, menu behavior and save/reload validation
remain pending; compilation and simulation tests do not establish game stability.
