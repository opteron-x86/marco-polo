# Skyrim 1.6.1170 engine boundary audit

The 0.1.0 reset detour used slot 0x8B from a misleading header comment. CommonLib's actual `TESObjectREFR::ResetInventory` dispatch selects 0x8A for SE/AE and 0x8B for VR. SE/AE slot 0x8B is GetFireNode and has a different signature and return value. A successful build and simulation tests did not detect this error.

The corrected code detours the engine's independent Address Library ID 19802. The 0.1.1 diagnostic build compared the current virtual entry to that ID and disabled the economy when Container Item Distributor's legitimate virtual override was present. Version 0.1.2 chains that virtual override separately, at the verified SE/AE slot 0x8A. Managed merchants bypass both the engine reset and the distributor's post-reset reroll; other references run the existing chain. Engine and third-party callbacks run without the economy mutex held.

On 2026-10-24, `tools/audit-runtime.py` checked the following against the pinned CommonLib source, the 1.6.1170 Address Library and a local SkyrimSE.exe. Executable instructions are packed, so this check verifies vtable destinations, not machine-code prologues or live behavior.

| Hook | SE/AE slot | AE ID | RVA |
| --- | --- | --- | --- |
| TESObjectREFR::ResetInventory | 0x8A | 19802 | 0x2ECDA0 |
| BarterMenu::Accept | 0x01 | 50947 | 0x8EB2C0 |
| BarterMenu::AdvanceMovie (inherited) | 0x05 | 82307 | 0xFAEDB0 |
| TESDescription::GetDescription | direct | 14552 | 0x1DBCC0 |

The previous reset target was ID 19994, RVA 0x2F39D0 (GetFireNode). A subsequent read-only inspection of the running game confirmed its `xor eax,eax; ret` implementation and the real ResetInventory entry point. Replacing a pointer-returning function with a void callback violates its ABI. This is a definite plugin defect. The reported save still failed after 0.1.1 disabled the barter hooks and after the DLL was removed for a diagnostic launch. The effect on existing save contents remains unresolved; no claim is made that every affected save is recoverable.

Oathvein's installed barter SWF was inspected locally. SkyUI loads `_config` asynchronously and then appends `BarterDataSetter`; `addDataProcessor` simply appends to that ordered list. Version 0.1.2 waits for `_config` and the barter multipliers before appending the economy formatter. The native 1.6.1170 ItemSelect callback was also inspected: it reads the passed unit price, truncates it to an integer, multiplies by the quantity and performs the transfer. The adapter's final gold reconciliation remains necessary for integral stack quotes. No executable or SWF data is distributed in this repository.

Six preserved save pairs from the reported session were checked locally. Main-save decompression, section boundaries and change-record decompression passed. ReSaver read all six without its broken-save or string-table-corruption flags. SKSE plugin/record boundaries and Marco Polo JSON parsing also passed. Originals were not edited. These checks do not establish that Skyrim can load every record successfully.

The user confirmed that a pre-installation save loads with Marco Polo's DLL absent, while the affected test save did not. A crash captured with the DLL absent still contained the invalid form pointer `0xD821` during inventory initialization. That rules out an executing Marco Polo hook in that particular crash, but does not establish how the bad state originated.

The 0.1.2 game test used Oathvein, the Missives addon and the user's existing mod profile, with Container Item Distributor and the two main Narrative Loot entries disabled by the user. Single-item purchases and sales showed matching list prices, item-card quotes and gold transfers. A 15-iron purchase quoted 493 gold; the following transaction's starting balances confirmed exactly 493 gold transferred after native integer rounding was reconciled. A saved day-3 economy containing one merchant and 51,824 JSON bytes passed core validation and an exact persistence round-trip.

The first restart/load of that fresh save restored the economy record, then crashed while a RaceMenu equipment-dye task traversed a Whiterun guard's skeleton. The user confirmed that a second restart loaded the same save successfully; subsequent trades were logged. A preceding crash occurred during actor inventory reset and leveled-item generation. These failures remain unresolved. Their call stacks identify where invalid memory was used, not which component caused it. A live read-only check of 0.1.2 confirmed the ResetInventory jump enters Marco Polo and GetFireNode retains its original implementation. Successful co-save restoration alone is not a successful game-load test, and the successful retry is not a stability guarantee.

Run the CommonLib check in CI after configuring the Windows build:

```sh
python tools/audit-runtime.py --commonlib build/windows-release/_deps/commonlibsse-src
```

Add `--exe /path/to/SkyrimSE.exe --address-library /path/to/versionlib-1-6-1170-0.bin` for the local executable check. Game files are never included in artifacts.
