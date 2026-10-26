# Game testing for 0.6.0

## Installation and save checks

Install the updated core with Skyrim closed. The Trade Rumors addon is unchanged.
Keep the test save’s ESS and matching SKSE co-save together.

1. Load the existing test save and allow a few seconds of unpaused play. The
   Right Shift + = shortcut should do nothing, and Marco Polo should have no
   pages in SKSE Menu Framework, even if the framework is installed for other mods.
2. Buy an item, ask for news, then make a new manual save. Quit to desktop,
   restart, and reload it. Confirm inventory and gold are retained and asking
   the same innkeeper on the same game day repeats the saved news.
3. Switch characters or start a fresh game. It must not inherit the previous
   character’s market history or daily rumor batch.

Economy schema 4 adds saved site observations; earlier schemas migrate without
resetting markets. Older DLLs cannot load new schema-4 saves. Use separate test
saves. Rumor history retains its format. Previous save/load failures were not
conclusively diagnosed; this build does not claim to repair them.

## Route security

See [the prototype report](SECURITY-SIMULATION.md) for supported sites, automated
results, and the camp/fort comparison to perform in game. This feature does not
add new dialogue topics or location-specific rumors yet.

The [merchant experiment](TRANSPORT-SIMULATION.md) covers the current cargo
allocator. Compare actual shop inventories and quotes after allowing several
days for deliveries. Clearing a camp need not reduce every price: exporters can
become dearer, and well-supplied markets may barely change. Existing shipments
retain their original arrival dates and losses when updating from 0.5.0.

## Innkeeper conversations

Install the optional addon and enable `MarcoPolo - Trade Rumors.esp`.
The existing [voice issues](RUMOR-VOICES.md) remain.

- Ask an innkeeper **Any news of trade?** The vanilla acknowledgement should
  finish before one news popup appears. Click OK and check that normal dialogue
  continues. Try letting the reply finish and skipping it on separate requests.
- Hulda and Keerava use **Sure.** Wilhelm and Hadring use **Let’s see here...**
  Delphine uses **Certainly.** when acting as an innkeeper. These choices depend
  on voice type, not NPC identity. Custom voices without the corresponding
  recording should retain the subtitle and reach the popup.
- Early in a new economy, expect local news. After five or six game days,
  connected distant holds should appear with dated, approximate price reports.
  Allow a few seconds of unpaused play after waiting for daily catch-up.
- Repeating a request that day, including at another inn in the same hold,
  should repeat the batch. Save/reload must not reroll it. The next day can
  produce different news.
- Check a replacement or mod-added innkeeper using the normal innkeeper or
  room-rental factions. Ordinary merchants do not offer this topic yet.

With debug logging enabled, `Trade-rumor dialogue:` lines record eligibility,
quest and response checks. An absent addon is reported separately at load.

## Trading

- Buy and sell single items and stacks. The list, item card, quantity total and
  actual gold change must agree. Cancelling must change neither items nor gold.
  Sell close to the merchant’s displayed funds to check low-gold transactions.
- Try drinks from the Rift to the Pale, weapons from Whiterun to Winterhold,
  and silver from the Reach to Haafingar. Record costs and receipts. Immediate
  local resale should lose money; supplying a market should narrow margins.
- Check household clutter, mod-added drinks, books, clothing, and stock from
  both merchant chests and shop displays. Legitimate offered stock must remain
  purchasable.
- Revisit after several days. Ordinary stock should respond to supply;
  protected quest, enchanted and custom items must retain their protections.

## Diagnostics

Set `debugLogging` to `true` in `SKSE/Plugins/MarcoPolo/settings.json`, restart,
and save. `MarcoPolo-state.json` and `MarcoPolo-information.json` should update
beside the SKSE logs without a popup. Both must parse as JSON; they contain the
same economy and information data passed to serialization for that save.
The economy file retains exact prices and stocks for debugging.

Visit a settlement, save, and inspect its reports in the information file.
Crossing a wilderness hold border alone must not reveal that hold’s market.
Older rumors must not replace fresher observations. Reloading should retain
remote report dates and the daily batch, although visiting the current hold
can refresh its local reports normally.

With logging disabled after restarting, saving should no longer update these
files. Existing diagnostic files are retained until replaced or removed.

Automated tests cover deterministic simulation, trading conservation, route
profits, information isolation, history bounds, rumor selection, persistence,
and malformed-data rollback. Binary checks validate the addon’s dialogue,
conditions, callback, startup SEQ and compiled script manifest. These checks
do not execute Skyrim.

Repeat dialogue, barter and save/reload on SE 1.5.97 and AE 1.6.x / 1.7.x with
matching SKSE and Address Library. Report the runtime, DLL version, NPC, action
and reproduction steps, with MarcoPolo.log and any crash report.
