Marco Polo — A Skyrim Economy Mod, 0.1.6.

Missives market bulletins and supply-order postings now stay on the board. Selecting a posting reads it in place. Close an order posting to accept it, or to deliver/cancel an already accepted order at its issuing board. Reading reports or using Take All does not accept an order.

Accepting an order gives you one carried contract paper with its order number and fixed terms. Delivery, cancellation and expiry remove it. Reading a carried paper offers no actions. Stored or dropped papers remain tied to their original order and read as closed after it ends. The issuing board can replace a lost paper. Loading cleans up legacy duplicate papers while preserving accepted orders, and restores a missing active paper.

Update the core and Missives addon together after exiting Skyrim. The addon now has 27 owned BOOK records, retaining the original IDs and adding nine board postings. It remains ESL-flagged, with no overrides, quests, dialogue or Papyrus scripts. Only Marco Polo papers are reconciled; other Missives jobs are left alone. The optional SKSE Menu Framework ledger still has no contract listings or actions.

No new game is needed. The economy co-save remains schema 2, with schema-1 migration retained. Regional trading balance is unchanged from 0.1.5. One DLL supports desktop SE 1.5.97 and AE 1.6.x / 1.7.x using alandtse CommonLibSSE-NG, matching SKSE and Address Library, without a runtime whitelist.

Merchant gold refreshes now use the same physical balance as transaction settlement. This prevents the menu from advertising money that the sale check cannot spend. A rejected sale reports the merchant's available gold and the sale total. No additional gold is created, and a sale is still refused when the merchant cannot pay in full.

Automated checks cover paper migration, accepted terms, exact-once delivery, cancellation, expiry, save/reload, receipt identity and stale confirmation rejection. Runtime audits check the new ContainerMenu slots and paired BookMenu relocation IDs. In-game validation of the new reading controls, Take All, receipt text and coexistence with other board mods remains pending; see docs/TESTING.md.
