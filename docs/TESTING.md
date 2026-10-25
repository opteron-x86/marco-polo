# Game testing for 0.1.8

The current build contains regional trading and inventory simulation, with an optional SKSE Menu Framework market ledger. It has no addon ESP, board integration, paper management or supply contracts.

Automated tests cover deterministic simulation, save restoration and migration, invalid-data rollback, stock conservation, exact stack pricing, route profits, finite cash, Speech ordering and five simulated years. Windows compilation and runtime audits verify the native adapter's build and relocation coverage. These checks do not execute Skyrim.

## Focused game checks

Use a new test game with the core enabled and the old Marco Polo addon removed. Keep the matching SKSE and Address Library for your runtime. Enable debug logging to record transactions.

1. Open a merchant and compare list, card and quantity prices. Buy and sell one item, then a stack. The actual gold change must equal the quoted total. Cancelling must leave goods and gold unchanged.
2. Sell near the merchant's cash limit. The displayed gold and available funds in the log must agree. A sale should succeed when the merchant can cover its full price.
3. Buy drinks in the Rift and sell in the Pale, weapons from Whiterun to Winterhold, or silver from the Reach to Haafingar. Record actual purchase and sale totals and travel costs. More supply should narrow later offers; buying and immediately reselling locally should lose money.
4. Check household clutter, mod-added alcohol, books, clothing and mixed chest/display stock. Supported goods should respond to local supply, and all legitimate stock must remain purchasable.
5. Wait several game days and revisit merchants. Ordinary stocks should respond to supply; unique, quest, enchanted and custom items should retain their protections.
6. Save to a new slot, exit to desktop, relaunch and load. Confirm restoration in the log and in the market ledger. Preserve the save/co-save pair if loading fails; a parseable co-save alone does not establish a working game save.
7. Check the optional market ledger, then repeat without it. Repeat startup, barter, restock and save/reload on supported SE 1.5.97, AE 1.6.x and AE 1.7.x runtimes.

The user's previous tests confirmed profitable trading and the merchant-gold log correction. General save/load stability remains unresolved. The latest reported load crash repeats the invalid inventory-ownership pointer previously seen with Marco Polo absent; see [the engine audit](ENGINE-AUDIT.md). No further board testing is requested.

Report the DLL version, Skyrim runtime, merchant, item, quantity, displayed price, actual gold change and reproduction steps. Include MarcoPolo.log and the crash report when available.
