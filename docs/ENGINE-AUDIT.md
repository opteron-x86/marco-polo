# Skyrim 1.6.1170 engine boundary audit

The 0.1.0 reset detour used slot 0x8B from a misleading header comment. CommonLib's actual `TESObjectREFR::ResetInventory` dispatch selects 0x8A for SE/AE and 0x8B for VR. SE/AE slot 0x8B is GetFireNode and has a different signature and return value. A successful build and simulation tests did not detect this error.

The corrected code selects 0x8A and checks that the vtable entry equals the independent Address Library ID 19802 before creating the detour. If it does not match, the economy is disabled without installing barter hooks.

On 2026-10-24, `tools/audit-runtime.py` checked the following against the pinned CommonLib source, the 1.6.1170 Address Library and a local SkyrimSE.exe. Executable instructions are packed, so this check verifies vtable destinations, not machine-code prologues or live behavior.

| Hook | SE/AE slot | AE ID | RVA |
| --- | --- | --- | --- |
| TESObjectREFR::ResetInventory | 0x8A | 19802 | 0x2ECDA0 |
| BarterMenu::Accept | 0x01 | 50947 | 0x8EB2C0 |
| BarterMenu::AdvanceMovie (inherited) | 0x05 | 82307 | 0xFAEDB0 |
| TESDescription::GetDescription | direct | 14552 | 0x1DBCC0 |

The previous reset target was ID 19994, RVA 0x2F39D0 (GetFireNode). This is a definite plugin defect. Its full effect on the reported load failures still requires an in-game retest; no claim is made that every affected save is recoverable.

Six preserved save pairs from the reported session were checked locally. Main-save decompression, section boundaries and change-record decompression passed. ReSaver read all six without its broken-save or string-table-corruption flags. SKSE plugin/record boundaries and Marco Polo JSON parsing also passed. Originals were not edited. These checks do not establish that Skyrim can load every record successfully.

Run the CommonLib check in CI after configuring the Windows build:

```sh
python tools/audit-runtime.py --commonlib build/windows-release/_deps/commonlibsse-src
```

Add `--exe /path/to/SkyrimSE.exe --address-library /path/to/versionlib-1-6-1170-0.bin` for the local executable check. Game files are never included in artifacts.
