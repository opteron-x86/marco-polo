# Marco Polo — Missives addon

Use with Marco Polo 0.1.7 and Missives. Enable `Marco Polo - Missives.esp` after Missives. It is ESL-flagged, has no overrides, and adds only 27 book records for the nine capital boards. The ESP is unchanged from 0.1.6; upgrades from earlier versions need both the core and addon.

Select market reports to read current conditions on the board. A shortage can produce a supply contract with an exact requested item, count, fixed payment and deadline. Read and close the posting to accept it. Both kinds of posting stay on the board.

With QuickLoot IE's optional API 2.0 or newer, Marco Polo postings are hidden from its loot list and cannot be taken through Take All. Open the board's regular inventory to read them. Ordinary Missives and other mods' notes keep their usual QuickLoot behavior. Without that API, deferred cleanup still removes any Marco Polo postings transferred by other loot menus.

Acceptance gives you one carried paper bearing the order number. It keeps the accepted terms and can be read while traveling. Return to the issuing board and read its order posting to deliver or cancel. Delivery, cancellation and expiry remove the carried paper. A stored or dropped paper stays tied to its original order and reads as closed after that order ends. Read the accepted posting at its board to replace a lost paper.

Existing duplicate reports and contract papers in your inventory are cleaned up on load. Active accepted orders keep their terms and receive one paper; loading also restores a missing active paper. No new game is needed. Papers left in other containers are cleaned up when brought back into your inventory.

Contract postings and actions are available only through this integration. The optional SKSE Menu Framework ledger displays markets without contract listings or buttons.

The DLL adds/removes only its own notes from the original board references. No Missives scripts or quest lists are replaced. Compatibility with additional notice mods and board replacements is part of the initial game-test checklist. Custom/new boards and Solstheim boards are not supported by this first adapter.
