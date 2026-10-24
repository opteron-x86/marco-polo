Marco Polo — A Skyrim Economy Mod, 0.1.2.

Critical correction to 0.1.0: the merchant inventory reset detour used the VR vtable slot on SE/AE, intercepting GetFireNode instead. The corrected detour targets the verified engine ResetInventory address directly. Existing virtual overrides, including Container Item Distributor's reset callback, are chained separately. The 0.1.1 diagnostic build disabled the economy when it encountered that override. CI verifies the slots against the pinned CommonLib dispatch; hook addresses were also audited against a local 1.6.1170 executable. Replace the old DLL and restart Skyrim completely.

Price formatting now attaches after SkyUI has loaded its configuration and registered its vanilla barter formatter. Attaching too early let the vanilla formatter overwrite economy prices in the item list. Exact list/card/gold agreement still needs an in-game retest. Logs survive restarts and include save/load and barter diagnostics. Disabling the economy in settings now skips native hook installation. Existing save data uses the same schema; no save conversion or cleaning is required by this update.

Requires Skyrim SE Steam 1.6.1170, matching SKSE/Address Library and a compatible SkyUI barter menu. Install the main ZIP. The Missives ZIP is optional and requires Missives; symbols and source are for development.

Native simulation tests and the Windows build are automated. In-game validation remains necessary for loading affected saves, menu quotes versus gold, merchant resets and compatibility with other mods. Keep the original save and co-save and use a separate test save. The core needs no ESP or Papyrus; the Missives addon supplies an ESL-flagged ESP without scripts or dialogue.
