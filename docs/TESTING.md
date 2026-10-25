# Game testing for 0.2.1

First check the dialogue correction: upgrade both the core and Trade Rumors addon,
restart Skyrim completely, load the current 0.2.0 save and talk to an innkeeper
such as Hulda. **Any news of trade?** should appear and open a news popup when
selected. This patch retains all form IDs and requires no new game or quest reset.
If it is still missing, leave `debugLogging` enabled: each opened conversation
records a `Trade-rumor dialogue:` line with speaker, eligibility, global, quest,
conditions and record linkage. An absent addon is reported separately at load.

The 0.2 series adds remembered markets and optional innkeeper rumors. Prices, merchant
funding, inventories and service costs retain their previous behavior. The core
remains DLL-only; the new dialogue addon contains an ESL-flagged ESP and two small
Papyrus scripts. There is no board integration or paper management.

Automated tests cover unchanged deterministic simulation, trading conservation,
route profits, information isolation, history bounds, dated rumors, anti-reroll
behavior, save restoration and invalid-data rollback. Binary checks validate the
addon dialogue, conditions, callback, startup SEQ and compiled script manifest.
Windows compilation and address-table audits do not execute Skyrim.

## Remembered markets and rumors

Install both matching archives, enable `MarcoPolo - Trade Rumors.esp`, and use
SKSE Menu Framework 3 for the tracker. Existing 0.1.8 economy saves can initialize
an empty information history; a fresh test character is also suitable. Fully
restart Skyrim after installing the DLL.

1. Enter Whiterun or the Bannered Mare. Allow about two seconds of unpaused play,
   then open **Marco Polo → Trade Status**. Whiterun should have current reports;
   unvisited markets should say **Unknown**. Opening or switching the menu must
   not reveal or refresh other holds. Crossing a wilderness hold border alone
   must not reveal its market.
2. Ask Hulda **Any news of trade?** The conversation should open an unvoiced OK
   popup. After dismissing it, **Recent trade rumors** should show its reports.
   On the economy's first day, local news is expected: no distant historical
   prices exist yet. The popup must work without Fuz Ro D-oh or Menu Framework;
   the tracker itself requires Menu Framework.
3. Let five or six game days pass, allowing a few seconds of unpaused play after
   waiting. Ask again. Connected distant holds should now have a few approximate
   commodity ranges, dated earlier than the day you heard them. Other commodities
   in those holds should remain unknown. Asking again that day or asking another
   innkeeper in the same hold must repeat the batch and preserve its dates.
4. Record a report from one hold, travel to another, and let several days pass.
   The first hold's report should retain its value and date. At seven days old it
   should say **stale**. A return visit or barter refreshes that hold's reports.
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
