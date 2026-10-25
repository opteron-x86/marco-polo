# Marco Polo
### A Skyrim Economy Mod

An SKSE economy inspired by the old DOS game **Marco Polo**. Regional markets produce, consume and exchange goods. Merchant prices and ordinary stock respond to the economy, and player trades feed back into it.

**0.2.2 adds vanilla voiced replies before innkeeper trade rumors.** The 0.2 series introduces remembered markets and trade rumors: learn local rates by visiting towns and trading, gather dated news at inns, and plan journeys using the reports you have collected. See [the release notes](docs/RELEASE.md) and [game test checklist](docs/TESTING.md) for voice selection and timing checks.

## Install

Built for **Skyrim SE 1.5.97 and AE 1.6.x / 1.7.x**, using one DLL and alandtse CommonLibSSE-NG. Install the matching **SKSE and Address Library** for your executable, plus **SkyUI** with a compatible barter menu. There is no single-version whitelist. Previous Marco Polo game checks used 1.6.1170; broader loading and relocation audits are described in [the engine audit](docs/ENGINE-AUDIT.md).

Install `Marco-Polo-<version>-<commit>.zip` with MO2 or Vortex. The core has **no ESP and no Papyrus scripts**.

**Optional SKSE Menu Framework 3:** open **Marco Polo → Trade Status** for remembered rates, report dates, sources, recent rumors and possible trade routes. Risa can provide access to SKSE Menu Framework; there is no separate Risa page. Live warehouse stocks and current prices for every hold are available only on **Live markets (debug)** when `debugLogging` is enabled.

**Optional innkeeper dialogue:** install the matching `Marco-Polo-<version>-<commit>-rumors.zip` and enable **MarcoPolo - Trade Rumors.esp**. Ask an innkeeper **Any news of trade?** Their short vanilla reply finishes before the news popup opens; dismiss it with OK. Reports appear in Trade Status. Replies match the speaker's voice type, with **Of course.** as the default. Custom voices without the vanilla recording retain its subtitle. This addon contains an ESL-flagged ESP, a startup SEQ and two small Papyrus scripts; its only master is Skyrim.esm. It does not require Missives or a voice/silence mod. The popup works without Menu Framework; reviewing the tracker requires Menu Framework.

Use a separate test save/profile. Keep the `.skse` co-save alongside its matching Skyrim save. Existing games initialize at the current game day, without simulating the character's entire past.

## The economy

Ten markets cover the nine holds and Solstheim. They track 25 commodities, finite warehouse stock, production, consumption, spoilage and steel processing. Regional specialties, seasons and Civil War demand affect supply and prices. Twenty-six directed routes move finite shipments with travel time, capacity and saved loss outcomes.

Each region has underlying production costs and import needs, expressed as `priceFactors`. Stocks move prices around those values within the configured bounds. The Rift exports drinks, Whiterun exports weapons, the Reach exports precious metals, and northern markets depend more on imports. Production, demand, NPC shipments and player deliveries change the margins.

Trades follow a supply curve: bulk quotes use the same curve and per-unit rounding as individual trades. Speech, perks and merchant effects remain inputs through the game's live barter multipliers. By default, raising each multiplier to the power `0.2` compresses the vanilla starting-Speech spread to approximately 1.25 times local value when buying and 0.80 when selling. A minimum 5% margin on either side prevents bonuses from producing immediate local resale profits. The list, card and quantity total share the quote calculation; actual inventory movement must occur before gold rounding is reconciled and a transaction is recorded.

Merchants register on first contact. Background customers buy ordinary stock, and merchants pay for replenishment from the regional warehouse. The next visit applies that ledger to their chest. Quest items, enchanted/custom instances and unknown extra data are protected from automatic inventory management. Base-form values and global barter settings are not rewritten.

Goods displayed around a shop remain available through Skyrim's barter menu. These purchases affect regional supply and prices, but Marco Polo does not move, duplicate or replenish the display objects.

Household goods include plates, bowls, cutlery and other items tagged `VendorItemClutter`, including those added by mods. Explicit mappings cover untagged vanilla silverware, linen wraps, charcoal and Hearthfire Extended rugs. Each item's value still contributes to its offer. Unknown miscellaneous items remain outside the economy unless classified; gold, lockpicks and quest items retain their exclusions or protections.

Drink includes ale, mead, wine, brandy and alcoholic drinks from Hearthfire and Dragonborn. Mod-added alcohol tagged `OCF_AlchDrinkAlcohol` or `VendorItemDrinkAlcohol` (including Moderate/Strong variants) joins the same market. Those keyword providers remain optional. Quest instances are still protected. Previously registered bottles move from Food to Drink accounting when their merchant is observed again, without changing their physical count.

Regional production and NPC consumption are explicit sources and sinks. This release does not simulate physical caravans, individual NPC households, service prices, training or house purchases.

## Learning about markets

Markets begin unknown. Entering a location marked as a city, town or inn records that hold's regional rates. The report updates on a new settlement visit, on a new game day while there, when opening barter, and after confirmed trades. Crossing a wilderness hold boundary alone does not reveal its markets. These are commodity rates, not saved item-by-item merchant offers.

Distant reports retain their recorded values. The tracker shows when the prices were observed and, for rumors, when and from whom you heard them. Reports at least seven calendar days old are marked stale but remain available. Rates use 100% as the regional reference; rumor ranges are approximate. Possible routes compare only known reports, including their age and uncertainty. They are leads, not guaranteed profits or current executable quotes.

Innkeepers in a hold share a daily batch of up to three reports from connected holds. News is drawn from actual earlier market samples, delayed by the incoming trade route's travel time (at least one day). Repeated requests, changing innkeepers within a hold and save/reload repeat the same batch. Older rumors never overwrite fresher firsthand information. A new economy has no earlier history; until distant news arrives, innkeepers discuss local prices. No reports are invented for the character's past.

The tracker retains the latest report for each commodity and each hold's latest rumor batch. There are no paper items or growing inventory stacks. Information gathering does not change prices, supply, merchant gold, the economy RNG or service costs.

## Settings and diagnostics

Files live under `SKSE/Plugins/MarcoPolo/`:

| File | Controls |
|---|---|
| `settings.json` | Enabled state, inventory management, transaction logging. |
| `economy.json` | Markets, production, demand, routes, recipes and balance limits. |
| `classification.json` | Exact forms, keyword/type fallbacks and exclusions. Keys use `Plugin.esp\|LocalFormID`. |

Restart Skyrim after edits. Saved market/commodity definitions must remain compatible. Invalid definitions and unsupported save schemas are logged and disabled. `manageInventory: false` leaves replenishment to the game for price-only compatibility testing. Mods that replace barter transactions or merchant restocking need compatibility testing. Unknown barter UIs retain game prices and produce a notification; inventory simulation can still run.

`economy.json` policy fields `barterExponent` and `minimumBarterMargin` control the spread. Set them to `1` and `0` to retain the game's original multipliers. Market `priceFactors` default to `1` when omitted, so existing custom definitions remain valid. These settings affect classified goods; unclassified goods use the game's pricing.

The co-save stores stocks, merchants, shipments, time and RNG state. A separate versioned record stores remembered reports, daily rumor batches and up to 32 days of market history. Loading a save restores both; switching characters clears the previous session. Missing or invalid information data starts a new information history without resetting the trading economy. `MarcoPolo.log` is beside the SKSE logs. With `debugLogging` enabled, the debug page exports `MarcoPolo-state.json` and `MarcoPolo-information.json` there.

Version 0.2.0 retains economy schema 3 from 0.1.8 and adds information schema 1 in its own co-save record. Existing economies begin with unknown markets and gather reports through play. Schema-1/2 market data can still be imported, but obsolete contract data is discarded. DLLs before 0.1.8 cannot read economy schema 3; 0.1.8 ignores the new information record. Remove the old Marco Polo Missives addon when setting up a new game; it is no longer shipped or used. Upgrades from before 0.1.5 retain the immediate narrower barter spread, while existing regional prices converge over subsequent game days. Loading schema 1 initializes only the household-goods stock introduced in 0.1.4.

## Build

Portable simulation and sanitizer tests:

```sh
cmake --preset native-tests
cmake --build --preset native-tests
ctest --preset native-tests --verbose
python3 -m unittest discover -s tests -p '*_test.py' -v
```

In debugger/ptrace environments, LeakSanitizer may require `ASAN_OPTIONS=detect_leaks=0`. CI keeps leak detection enabled.

Windows: use an x64 Visual Studio developer shell, clang-cl, Ninja and the pinned vcpkg baseline, with `VCPKG_ROOT` set:

```powershell
cmake --preset windows-release
cmake --build --preset windows-release
ctest --test-dir build/windows-release --verbose
python tools/package.py --build build/windows-release --output dist
```

The simulation can run without Skyrim:

```sh
build/native-tests/marco_sim package/SKSE/Plugins/MarcoPolo/economy.json 365 42 > markets.csv
```

See [architecture](docs/ARCHITECTURE.md), [release automation](docs/RELEASING.md) and [dependency notices](THIRD_PARTY_NOTICES.md). Licensed GPL-3.0 with the additional permissions in [EXCEPTIONS.md](EXCEPTIONS.md).
