# Marco Polo
### A Skyrim Economy Mod

An SKSE economy inspired by the old DOS game **Marco Polo**. Regional markets produce, consume and exchange goods. Merchant prices and ordinary stock respond to the economy, and player trades feed back into it.

**0.1.2 is experimental and needs in-game testing.** It corrects a dangerous engine hook in 0.1.0, handles existing inventory-reset hooks, and fixes price-formatter initialization order. Replace the older DLL before further testing. Automated tests exercise the model, save data and release tooling. They cannot verify Skyrim's menus, merchant resets or interactions with an installed mod list. See [the game test checklist](docs/TESTING.md).

## Install

Requires **Skyrim SE Steam 1.6.1170**, its matching SKSE and Address Library, and **SkyUI** with a compatible barter menu. Other runtime versions are deliberately rejected until their integration is verified.

Install `Marco-Polo-<version>-<commit>.zip` with MO2 or Vortex. The core has **no ESP and no Papyrus scripts**.

- **Optional Missives addon:** install `-missives.zip` and enable `Marco Polo - Missives.esp` after `Missives.esp`. This ESL-flagged plugin contains 18 new note records. It adds market bulletins and supply contracts to the nine capital boards, without dialogue, quests or scripts.
- **Optional SKSE Menu Framework 3:** open **Marco Polo → Market ledger** for stock, prices, contracts and diagnostic export. Risa can provide access to SKSE Menu Framework; there is no separate Risa page.

Use a separate test save/profile. Keep the `.skse` co-save alongside its matching Skyrim save. Existing games initialize at the current game day, without simulating the character's entire past.

## The economy

Ten markets cover the nine holds and Solstheim. They track 24 commodities, finite warehouse stock, production, consumption, spoilage and steel processing. Regional specialties, seasons and Civil War demand affect supply and prices. Twenty-six directed routes move finite shipments with travel time, capacity and saved loss outcomes.

Prices move gradually within configured bounds. Trades also follow a supply curve: bulk quotes use the same curve and per-unit rounding as individual trades. The item's original value, Speech and barter multipliers remain inputs. The list, card and quantity total share the quote calculation; actual inventory movement must occur before gold rounding is reconciled and a transaction is recorded.

Merchants register on first contact. Background customers buy ordinary stock, and merchants pay for replenishment from the regional warehouse. The next visit applies that ledger to their chest. Quest items, enchanted/custom instances and unknown extra data are protected from automatic inventory management. Base-form values and global barter settings are not rewritten.

Regional production and NPC consumption are explicit sources and sinks. This release does not simulate physical caravans, individual NPC households, service prices, training or house purchases.

## Notices and contracts

Market bulletins describe current conditions. Shortages can produce supply orders for an **exact item**, quantity, fixed reward and deadline. Read and close a contract note to accept, deliver or cancel it. Delivery requires the complete order near the issuing board. It removes the goods, supplies the hold and pays once. Treasury funds are reserved at posting and released on cancellation or expiry. Each hold issues at most one new order per game day.

Copies show the latest posting. Only Marco Polo notes are added or removed; Missives scripts and quest lists are untouched. Notes and Announcements and board replacers still need coexistence testing. New/custom boards and Solstheim boards need an adapter.

## Settings and diagnostics

Files live under `SKSE/Plugins/MarcoPolo/`:

| File | Controls |
|---|---|
| `settings.json` | Enabled state, inventory management, transaction logging. |
| `economy.json` | Markets, production, demand, routes, recipes and balance limits. |
| `classification.json` | Exact forms, keyword/type fallbacks and exclusions. Keys use `Plugin.esp\|LocalFormID`. |

Restart Skyrim after edits. Saved market/commodity definitions must remain compatible. Invalid definitions and unsupported save schemas are logged and disabled. `manageInventory: false` leaves replenishment to the game for price-only compatibility testing. Mods that replace barter transactions or merchant restocking need compatibility testing. Unknown barter UIs retain game prices and produce a notification; inventory simulation can still run.

The co-save stores stocks, merchants, shipments, contracts, time and RNG state. Loading a save restores its economy; switching characters clears the previous session. `MarcoPolo.log` is beside the SKSE logs. With `debugLogging` enabled, the optional ledger can export `MarcoPolo-state.json` there.

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
