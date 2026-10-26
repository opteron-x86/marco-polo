# Game testing for 0.7.0

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

The [campaign report](CAMPAIGN-SIMULATION.md) describes the 39 supported locations
and coverage limits. With debug logging enabled, a fresh session should report
40 configured profiles, including the inactive Bleak Falls Barrow control.

- Save before clearing a supported camp such as Embershard, Valtheim or
  Knifepoint Ridge. Clear it normally, leave the location, and allow unpaused
  time for observation. The log should name that site changing to `cleared`.
  A dead boss alone is insufficient if Skyrim has not marked the location clear.
- Compare a few days of local and connected shop prices against the uncleared
  save. Deliveries take time, other camps still matter, and exporters may become
  dearer. A single clear need not produce a visible price change everywhere.
- Check Fort Greymoor before and after a normal Civil War occupation. Its log
  should distinguish hostile, cleared, and the occupying military faction.
  Repeat with Fort Amol or another newly covered fort: unverified initial
  occupants remain inactive until military control is recognized.
- Clearing Bleak Falls Barrow or another crypt must not add supplies or protect
  routes. Karthspire is excluded because its location lacks the clearable tag.
- Save and reload after a clear or occupation. The site state and pending dates
  must survive; cargo already traveling keeps its original arrival and loss.

Occupation overhauls can change the vanilla keyword meanings; capture the log
and mod list if a fort fails to change. No location-specific rumors were added.

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
- Visit an import-dependent shop, buy down a tracked category, and revisit on
  several later days. Some replacement stock should arrive while supply and
  merchant funds permit it. Check Winterhold equipment or northern provisions
  in particular. This does not promise a full restock or new item types.
- Compare the merchant's displayed gold with the amount available for a sale
  after repeated visits. Funds should not reset to their original value each day.

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
