Marco Polo — A Skyrim Economy Mod, 0.1.3.

Fixes purchases rejected for books, clothes and other goods sold from shop displays. The previous stock check counted only the hidden merchant chest. Skyrim's sale inventory can combine that chest with objects elsewhere in the shop, so an item could appear for sale but be rejected, or stop selling after the chest portion ran out. The adapter now checks the selected sale entry and accounts for the actual source after Skyrim performs the transfer. Display stock affects supply and prices without being duplicated into the chest or automatically replenished.

Includes the 0.1.2 engine-hook and price-display corrections. Version 0.1.0 intercepted GetFireNode instead of ResetInventory on SE/AE. The corrected hook targets the verified engine address and chains existing virtual overrides. Price formatting attaches after SkyUI configuration. Diagnostic logs survive restarts and now distinguish offered quantity, chest stock, external purchases and final gold changes.

Requires Skyrim SE Steam 1.6.1170, matching SKSE/Address Library and a compatible SkyUI barter menu. Exit Skyrim completely before updating the main ZIP. The Missives ZIP is optional and requires Missives; symbols and source are for development. Existing schema-1 economy saves load without cleaning or manual conversion.

Automated tests cover the simulation, persistence and display-stock accounting. In-game testing is still required. The 0.1.2 session confirmed corrected prices and a successful fresh-save reload on retry, but also produced intermittent loading crashes. The original affected Markarth saves remain unresolved. Keep their save/co-save pairs and continue with a separate test character. Container Item Distributor compatibility is not yet verified in game.

The core needs no ESP or Papyrus; the Missives addon supplies an ESL-flagged ESP without scripts or dialogue.
