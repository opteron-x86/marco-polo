# Game testing for 0.3.1

## Trade Ledger

Install the updated core and restart Skyrim; the Trade Rumors addon is unchanged.
Load your existing test save and allow two seconds of unpaused play.

1. Press **Right Shift + =** during normal play. The ledger should pause the game, show a
   cursor and use the same fonts as your installed UI. Compare both the title
   and row text. Test with the current font replacement, then vanilla fonts if
   available. No font files or font configuration should be overwritten.
2. Use Markets, Routes and Rumors. Click the hold name to choose a hold, use
   the page arrows or mouse wheel, and hover a row for its source and date.
   Unvisited goods remain Unknown. No player-facing price percentages should
   appear; an old report stays old when you browse it.
3. Drag the title and resize with the bottom-right grip. Lock the window,
   confirm it cannot move or resize, then unlock it. Close with **Esc**, **Right Shift + =**
   or X. Movement, camera, dialogue and inventory controls should work normally.
4. Reopen several times, then restart Skyrim. Position, scale, lock and route
   bookmarks should persist. Try a different screen resolution and confirm
   the window stays on screen. Preferences are created at
   `SKSE/Plugins/MarcoPolo/ledger.json` (possibly in MO2's Overwrite).
5. Bookmark a route, switch characters and reopen. Only that character's known
   opportunities should appear. Opening the ledger must not create or refresh
   information. Save to a new slot after closing it, quit, and reload; the
   remembered markets should remain intact.
6. Check Right Shift + = while dialogue, barter, inventory, loading or the console is open:
   it should leave that menu alone. Equals by itself and Left Shift + Equals
   must not open the ledger. The ledger must not appear at the main menu.
   With a controller, shoulders switch tabs, left/right changes holds or pages,
   up/down changes pages, and B closes the ledger after opening it with Right Shift + =.

The keyboard shortcut uses `hotkey` and `hotkeyModifier` in `ledger.json`.
The defaults are 13 (Equals) and 54 (Right Shift), using SKSE keyboard scan codes.
Set `hotkey` to 0 to disable the shortcut, or `hotkeyModifier` to 0 for a single
key. Restart after editing. The old F8 default migrates automatically; custom
single-key bindings retain their behavior.
Delete only that preferences file to restore the default position and size.

Repeat the open/close and save/reload checks on SE 1.5.97 and AE 1.6.x / 1.7.x.
The native build and local AVM1 test player cannot verify Skyrim's font library
or input restoration. Log lines `Trade ledger registered` and `Trade ledger ready`
identify successful registration and movie initialization.

## Innkeeper replies

The existing addon has [known voice issues](RUMOR-VOICES.md); those are not
changed by this ledger build.

First check the voiced replies: upgrade both the core and Trade Rumors addon,
restart Skyrim completely and load the current 0.2.x save. No new game or quest
reset is required; existing form IDs and save formats are retained.

- Ask Elda **Any news of trade?** She should say **Of course.** in her own voice,
  then the news popup should open once. The old placeholder must not appear.
- Ask Hulda or Keerava: expect **Sure.** Ask Wilhelm or Hadring: expect
  **Let's see here...** Delphine should use **Certainly.** when acting as an
  innkeeper. If Nils is running Candlehearth Hall, he should offer to share what
  he knows. These choices use voice type, not NPC identity.
- Let each response finish, and try skipping a response on a separate request.
  The popup should follow the ended response, never cover the beginning of the
  line or open twice. Dismissing it must leave normal dialogue working.
- A mod-added innkeeper with a standard voice should get the matching reply.
  A custom voice lacking the vanilla recording should retain the default subtitle
  and still reach the popup; no silence mod is required. Verify this fallback in
  game before claiming support for a particular custom voice.

With `debugLogging` enabled, each opened conversation records a
`Trade-rumor dialogue:` line with eligibility, quest and linkage checks. A valid
speaker should have one matching INFO; the log includes its shared response ID.
An absent addon is reported separately at load.

The 0.2 series adds remembered markets and optional innkeeper rumors. Prices, merchant
funding, inventories and service costs retain their previous behavior. The core
contains the DLL and ledger movie; the new dialogue addon contains an ESL-flagged ESP and two small
Papyrus scripts. There is no board integration or paper management.

Automated tests cover unchanged deterministic simulation, trading conservation,
route profits, information isolation, history bounds, dated rumors, anti-reroll
behavior, save restoration and invalid-data rollback. Binary checks validate the
addon dialogue, conditions, callback, startup SEQ and compiled script manifest.
Windows compilation and address-table audits do not execute Skyrim.

## Remembered markets and rumors

Install both matching archives, enable `MarcoPolo - Trade Rumors.esp`, and press Right Shift + = for the tracker (SKSE Menu Framework remains an alternative). Existing 0.1.8 economy saves can initialize
an empty information history; a fresh test character is also suitable. Fully
restart Skyrim after installing the DLL.

1. Enter Whiterun or the Bannered Mare. Allow about two seconds of unpaused play,
   then open **Right Shift + = → Markets**. Whiterun should have current reports;
   unvisited markets should say **Unknown**. Opening or switching the menu must
   not reveal or refresh other holds. Crossing a wilderness hold border alone
   must not reveal its market.
2. Ask Hulda **Any news of trade?** Her voiced acknowledgement should finish,
   then the OK popup should open. After dismissing it, **Recent trade rumors** should show its reports.
   On the economy's first day, local news is expected: no distant historical
   prices exist yet. The popup must work without Fuz Ro D-oh or Menu Framework;
   the ledger also works without Menu Framework.
3. Let five or six game days pass, allowing a few seconds of unpaused play after
   waiting. Ask again. Connected distant holds should now have a few approximate
   commodity reports, dated earlier than the day you heard them. Other commodities
   in those holds should remain unknown. Asking again that day or asking another
   innkeeper in the same hold must repeat the batch and preserve its dates.
4. Record a report from one hold, travel to another, and let several days pass.
   The first hold's report should retain its value and date. At seven days old it
   should be marked **old** in the ledger. A return visit or barter refreshes that hold's reports.
   Older rumors must not overwrite fresher firsthand observations. Price-gap
   suggestions must use only remembered reports and show their ages.
5. Buy or sell an item. The local commodity report should reflect the resulting
   regional rate; actual item prices still include the merchant's offer. Confirm
   list, card, quantity total and actual gold change remain consistent.
6. Save to a new slot, quit to desktop, restart and reload. Remote reports, dates,
   rumor sources and the daily batch should survive. Reloading then repeating the
   same request must not generate different news. A local visit after loading may
   refresh the current hold normally. Keep each ESS with its matching SKSE co-save.
7. Switch to another character or start a fresh game: it must not inherit the
   previous character's reports. Check a non-innkeeper: the topic should be absent.
   Check another inn, including a replacement innkeeper or mod-added innkeeper in
   the normal innkeeper/room-rental factions.
8. With `debugLogging: true`, verify **Live markets (debug)** still exposes current
   global prices and stocks and exports both diagnostic JSON files. The ordinary
   Trade Status page must continue using remembered reports even with logging on.
   Without debug logging, the live page should be absent.

Repeat dialogue, popup, barter and save/reload on SE 1.5.97, AE 1.6.x and AE 1.7.x
with matching SKSE and Address Library. Normal quest dialogue must remain intact.
The new plugin uses old-runtime-compatible light-plugin IDs and form version 44.

## Trading regression checks

- Buy/sell one item and a stack; actual gold must match the quote. Cancellation
  must change neither items nor gold. Sell close to the displayed merchant funds.
- Try drinks from the Rift to the Pale, weapons from Whiterun to Winterhold, and
  silver from the Reach to Haafingar. Record actual costs and receipts. Immediate
  local resale should lose money and supplying a market should narrow margins.
- Check household clutter, mod-added drinks, books, clothing and mixed merchant
  chest/display stock. All legitimate offered stock must remain purchasable.
- Revisit after several days. Ordinary stock should respond to supply; protected
  quest, enchanted and custom items must retain their protections.

The previous save/load failures were not conclusively diagnosed; this build does
not claim to repair them. Testing the new dialogue and information record remains
necessary. Report the runtime, DLL version, NPC, menu action and reproduction
steps, with MarcoPolo.log and any crash report. Debug exports are
`MarcoPolo-state.json` and `MarcoPolo-information.json` beside the SKSE logs.
