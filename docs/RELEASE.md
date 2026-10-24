Marco Polo — A Skyrim Economy Mod, 0.1.1.

Critical correction to 0.1.0: the merchant inventory reset detour used the VR vtable slot on SE/AE, intercepting GetFireNode instead. The corrected hook checks the intended ResetInventory address before installation. CI now verifies the slots against the pinned CommonLib dispatch; all hook addresses were also audited against a local 1.6.1170 executable. Replace the old DLL and restart Skyrim completely.

Logs now survive restarts and include save/load and barter diagnostics. The reported list-price versus gold-deduction mismatch is still under investigation. Existing save data uses the same schema; no save conversion or cleaning is required by this update.

Requires Skyrim SE Steam 1.6.1170, matching SKSE/Address Library and a compatible SkyUI barter menu. Install the main ZIP. The Missives ZIP is optional and requires Missives; symbols and source are for development.

Native simulation tests and the Windows build are automated. In-game validation remains necessary for loading affected saves, menu quotes versus gold, merchant resets and compatibility with other mods. Keep the original save and co-save and use a separate test save. The core needs no ESP or Papyrus; the Missives addon supplies an ESL-flagged ESP without scripts or dialogue.
