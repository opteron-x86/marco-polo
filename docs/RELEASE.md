Marco Polo — A Skyrim Economy Mod, 0.1.7.

Fixes a crash when selecting a Marco Polo contract posting on a Missives board. The Read-button wrapper previously dereferenced a null item-card object during menu initialization. It now uses the selected posting's known book type and checks UI objects before calling them.

Accepted papers now recover their order number from the selected inventory instance when Skyrim omits the extra-data argument while opening the book. A single tagged carried copy can resolve a base-form-only inventory row. Untagged, ambiguous and obsolete papers cannot inherit a later order's terms. Diagnostic logging records how the paper was identified.

Adds optional QuickLoot IE API integration. Marco Polo market reports and board contract postings are excluded from QuickLoot's list and blocked from pickup, including Take All. Read them through the board's regular inventory. Other Missives jobs and carried receipts keep their normal behavior. There is no new required mod; deferred cleanup remains available when the API is absent.

Exit Skyrim before updating the core. No new game is needed, and accepted orders keep their terms. The Missives ESP is byte-for-byte unchanged from 0.1.6; users upgrading from earlier versions should update both packages. The economy co-save remains schema 2, with schema-1 migration retained. Trading balance is unchanged.

One DLL supports desktop SE 1.5.97 and AE 1.6.x / 1.7.x using alandtse CommonLibSSE-NG, matching SKSE and Address Library. This fix adds no native hook offsets, relocation IDs or runtime whitelist.

Regression checks exercise the actual Read-button helper with absent, null and stale item cards, plus receipt stacks with missing, duplicate, mixed and invalid identities. The reported QuickLoot-to-inventory-to-board sequence still needs in-game verification; see docs/TESTING.md.
