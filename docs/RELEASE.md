# Skyrim True Economy and Merchantry 0.4.0

The Trade Ledger, its keyboard shortcut, and the SKSE Menu Framework pages have
been removed. Market news remains available through innkeeper conversations in
the optional Trade Rumors addon.

Pricing, merchant inventories, simulation rules, and the saved economy and rumor
history are unchanged. Existing saves use the same record formats. The addon’s
plugin and scripts are unchanged, including its [known voice issues](RUMOR-VOICES.md).

Install the updated core with Skyrim closed. When merging into an older install,
remove `Interface/MarcoPoloTradeLedger.swf`. The old
`SKSE/Plugins/MarcoPolo/ledger.json` preferences are ignored and can be deleted.
Other mods may still need SKSE Menu Framework.

For diagnostics, enable `debugLogging` in `settings.json` and restart Skyrim.
Saving then writes `MarcoPolo-state.json` and `MarcoPolo-information.json` beside
the SKSE logs. These files replace the menu’s export button.
