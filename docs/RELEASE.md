Marco Polo — A Skyrim Economy Mod, 0.1.8.

Removes the Missives addon, board and book hooks, paper management, QuickLoot integration, and supply-contract system. The addon ESP, generator, tests and Nexus upload step are removed. Packages contain only the core mod, symbols and corresponding source.

Regional supply and demand, dynamic prices, merchant inventory management, profitable inter-hold trading, Speech/perk adjustments and the merchant-gold display correction remain. The optional SKSE Menu Framework market ledger remains available.

For a new game, install only the core and remove the old Marco Polo Missives addon. The core requires no ESP or Papyrus scripts. Saves now use schema 3 without contract state; schema-1/2 market and trading data can still be imported, with obsolete contract data discarded. Earlier DLLs cannot read schema 3.

Desktop SE 1.5.97 and AE 1.6.x / 1.7.x support remains, using alandtse CommonLibSSE-NG, matching SKSE and Address Library, without a runtime whitelist.

This removes the integration; it is not a claim to repair the separately reported save-load failures. Local save inspection found complete main-save decompression and parseable economy co-save records, while the latest crash repeated an inventory-reset failure previously reproduced with Marco Polo absent. Its cause remains unresolved.
