# Marco Polo
### A Skyrim Economy Mod

An SKSE economy inspired by the old DOS game **Marco Polo**. Regional markets produce, consume and exchange goods. Merchant prices and ordinary stock respond to the economy, and player trades feed back into it.

**0.1.5 makes regional merchanting viable and broadens runtime loading.** It narrows the barter spread, establishes regional export/import specialties and tracks vanilla/DLC alcohol plus supported mod keywords. Selected long routes return at least twice the purchase outlay in controlled simulation tests with starting Speech, finite merchant cash and travel time. Actual offers depend on merchants, stock, bonuses and elapsed time. See [the game test checklist](docs/TESTING.md).

## Install

Built for **Skyrim SE 1.5.97 and AE 1.6.x / 1.7.x**, using one DLL and alandtse CommonLibSSE-NG. Install the matching **SKSE and Address Library** for your executable, plus **SkyUI** with a compatible barter menu. There is no single-version whitelist. Previous Marco Polo game checks used 1.6.1170; broader loading and relocation audits are described in [the engine audit](docs/ENGINE-AUDIT.md).

Install `Marco-Polo-<version>-<commit>.zip` with MO2 or Vortex. The core has **no ESP and no Papyrus scripts**.

- **Optional Missives addon:** install `-missives.zip` and enable `Marco Polo - Missives.esp` after `Missives.esp`. This ESL-flagged plugin contains 18 new note records. It adds market bulletins and supply contracts to the nine capital boards, without dialogue, quests or scripts.
- **Optional SKSE Menu Framework 3:** open **Marco Polo → Market ledger** for stock, prices and diagnostic export. Contract postings and actions are available through Missives notes only. Risa can provide access to SKSE Menu Framework; there is no separate Risa page.

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

## Notices and contracts

With Missives and the optional addon installed, market bulletins describe current conditions and shortages can produce supply orders for an **exact item**, quantity, fixed reward and deadline. Read and close a contract note to accept, deliver or cancel it. Delivery requires the complete order near the issuing board. It removes the goods, supplies the hold and pays once. Treasury funds are reserved at posting and released on cancellation or expiry. Each hold issues at most one new order per game day. Without the Missives integration, no contracts are posted or offered through the ledger.

Copies show the latest posting. Only Marco Polo notes are added or removed; Missives scripts and quest lists are untouched. Notes and Announcements and board replacers still need coexistence testing. New/custom boards and Solstheim boards need an adapter.

## Settings and diagnostics

Files live under `SKSE/Plugins/MarcoPolo/`:

| File | Controls |
|---|---|
| `settings.json` | Enabled state, inventory management, transaction logging. |
| `economy.json` | Markets, production, demand, routes, recipes and balance limits. |
| `classification.json` | Exact forms, keyword/type fallbacks and exclusions. Keys use `Plugin.esp\|LocalFormID`. |

Restart Skyrim after edits. Saved market/commodity definitions must remain compatible. Invalid definitions and unsupported save schemas are logged and disabled. `manageInventory: false` leaves replenishment to the game for price-only compatibility testing. Mods that replace barter transactions or merchant restocking need compatibility testing. Unknown barter UIs retain game prices and produce a notification; inventory simulation can still run.

`economy.json` policy fields `barterExponent` and `minimumBarterMargin` control the spread. Set them to `1` and `0` to retain the game's original multipliers. Market `priceFactors` default to `1` when omitted, so existing custom definitions remain valid. These settings affect classified goods; unclassified goods use the game's pricing.

The co-save stores stocks, merchants, shipments, contracts, time and RNG state. Loading a save restores its economy; switching characters clears the previous session. `MarcoPolo.log` is beside the SKSE logs. With `debugLogging` enabled, the optional ledger can export `MarcoPolo-state.json` there.

Version 0.1.5 retains schema 2 and reads both schema-1 and schema-2 economies. No new game is needed. The barter spread changes immediately; saved regional prices remain intact and approach the new regional values as days pass. Allow roughly two in-game weeks for the new trading pattern to develop. Loading schema 1 initializes only the household-goods stock introduced in 0.1.4. Pre-0.1.4 DLLs cannot read schema 2.

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
