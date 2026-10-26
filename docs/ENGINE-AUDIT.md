# Skyrim desktop runtime and engine audit

Version 0.4.0 removes both market menus and their input handling. Barter hooks, rumor dialogue and co-save formats are unchanged. Debug exports now run when saving with debug logging enabled.

Version 0.2.0 adds report history and an optional native Papyrus callback for innkeeper rumors. It introduces no new detours. The popup uses pinned CommonLib's `MessageBoxMenu::Create` dispatch: SE ID 51421, earlier AE ID 52270 and post-1.6.1130/1.7.x ID 442726. The audit checks that call's availability alongside the existing trading hooks in supplied address tables. Innkeeper faction membership and location traversal use CommonLib APIs, with game access queued onto the game thread. The dialogue ESP uses form version 44 and light-plugin IDs 0x800–0x804, extended through 0x80A in 0.2.2.

Version 0.2.1 adds a debug-only dialogue-open event sink. Its queued task checks
the current speaker through `MenuTopicManager` (SE/AE IDs 514959/401099) and the
INFO conditions through `TESCondition::IsTrue` (29074/29888). The address-table
audit includes both calls. Quest and record-link checks use CommonLib members;
there are no new hooks, runtime whitelist entries or quest-state mutations.

Version 0.2.2 changes the INFO callback from Begin to End and shares vanilla
response data for voiced acknowledgements. The six added INFOs select by Skyrim's
GetIsVoiceType condition; the original INFO is the mutually exclusive default.
The diagnostic now checks all seven INFOs and logs the matching shared response.
It introduces no additional native calls or serialization changes. The user
confirmed that 0.2.1's topic and popup work with Elda; the new speech ordering
and additional voice routes still require in-game testing.

Version 0.1.8 removed the entire Missives integration, including its description/book detours, ContainerMenu hooks, QuickLoot callbacks, note records and contract system. Those integrations remain absent.

A brief check of the fresh E395B0C6 character found both new main saves completely decompressible and both SKSE co-save containers well-formed. Each Marco Polo record parsed as schema 2, day 1, with zero merchants and zero contracts. The 22:47:11 crash on 2026-10-24 repeated SkyrimSE+0x1E0950 with the invalid ownership pointer 0xE885 during actor inventory reset. This same signature previously reproduced with Marco Polo's DLL absent. The preceding 22:35:38 and 22:42:44 crashes instead dereferenced null at SkyrimSE+0x4FDACF. None establishes the originating writer or proves the saves can load. Original files were backed up and not edited; investigation stops at the user's request.

Version 0.1.5 removes the explicit 1.6.1170 load check and metadata whitelist. Like Quickslot and Tessera, it declares CommonLib runtime-selected layouts and exports both `SKSEPlugin_Query` and `SKSEPlugin_Version`. The pinned alandtse CommonLibSSE-NG 13.1.0 supports Address Library formats 1, 2 and 5. The inventory-reset detour uses `REL::RelocationID(19375, 19802)` for SE/AE; desktop virtual slots remain 0x8A, 0x01 and 0x05. The SE reset identity is recorded in alandtse's address database as `TESObjectREFR::ResetInventory_140298C80` at 0x140298C80.

`tools/audit-runtime.py` verifies the slots against CommonLib and can check supplied SE/AE CSV tables, compressed databases and format-5 dense tables. Local audits cover the hook relocations and barter target handle in SE 1.5.97, the supplied 1.6.x databases and AE 1.7.99 / 1.7.104. The DLL audit requires the legacy Query export, runtime-independent layout metadata, format-5 support and an empty whitelist. These checks establish loading/relocation coverage, not execution of Marco Polo's barter callbacks on every runtime. Earlier live evidence below remains specific to 1.6.1170.

In the 0.1.5 Thoring test, the screenshot showed 10 merchant gold and a 6-gold Nord Mead quote, while the matching 18:40:06 log counted one physical chest coin and rejected the sale. Earlier rejections also involved Shein, Honningbrew Mead and a leek. The native 1.6.1170 balance update reads `TESDataHandler::merchantInventory` and overwrites BarterMenu's cached balance before invoking `UpdatePlayerInfo`. Oathvein's installed barter and imported bottom-bar scripts were inspected: that callback sets both `_vendorGold` and its displayed field. Version 0.1.6 wraps the callback's balance arguments, preserving its merchant name and player stats, to use the physical player/chest funds used for settlement. It also refreshes balances on attachment and rejection. It adds no engine offset or relocation. The corrected display and affordability boundary need in-game confirmation.

## Earlier 1.6.1170 investigation

The 0.1.0 reset detour used slot 0x8B from a misleading header comment. CommonLib's actual `TESObjectREFR::ResetInventory` dispatch selects 0x8A for SE/AE and 0x8B for VR. SE/AE slot 0x8B is GetFireNode and has a different signature and return value. A successful build and simulation tests did not detect this error.

The corrected code detours the engine's independent Address Library ID 19802. The 0.1.1 diagnostic build compared the current virtual entry to that ID and disabled the economy when Container Item Distributor's legitimate virtual override was present. Version 0.1.2 chains that virtual override separately, at the verified SE/AE slot 0x8A. Managed merchants bypass both the engine reset and the distributor's post-reset reroll; other references run the existing chain. Engine and third-party callbacks run without the economy mutex held.

On 2026-10-24, `tools/audit-runtime.py` checked the following against the pinned CommonLib source, the 1.6.1170 Address Library and a local SkyrimSE.exe. Executable instructions are packed, so this check verifies vtable destinations, not machine-code prologues or live behavior.

| Hook | SE/AE slot | AE ID | RVA |
| --- | --- | --- | --- |
| TESObjectREFR::ResetInventory | 0x8A | 19802 | 0x2ECDA0 |
| BarterMenu::Accept | 0x01 | 50947 | 0x8EB2C0 |
| BarterMenu::AdvanceMovie (inherited) | 0x05 | 82307 | 0xFAEDB0 |

The previous reset target was ID 19994, RVA 0x2F39D0 (GetFireNode). A subsequent read-only inspection of the running game confirmed its `xor eax,eax; ret` implementation and the real ResetInventory entry point. Replacing a pointer-returning function with a void callback violates its ABI. This is a definite plugin defect. The reported save still failed after 0.1.1 disabled the barter hooks and after the DLL was removed for a diagnostic launch. The effect on existing save contents remains unresolved; no claim is made that every affected save is recoverable.

Oathvein's installed barter SWF was inspected locally. SkyUI loads `_config` asynchronously and then appends `BarterDataSetter`; `addDataProcessor` simply appends to that ordered list. Version 0.1.2 waits for `_config` and the barter multipliers before appending the economy formatter. The native 1.6.1170 ItemSelect callback was also inspected: it reads the passed unit price, truncates it to an integer, multiplies by the quantity and performs the transfer. The adapter's final gold reconciliation remains necessary for integral stack quotes. No executable or SWF data is distributed in this repository.

Six preserved save pairs from the reported session were checked locally. Main-save decompression, section boundaries and change-record decompression passed. ReSaver read all six without its broken-save or string-table-corruption flags. SKSE plugin/record boundaries and Marco Polo JSON parsing also passed. Originals were not edited. These checks do not establish that Skyrim can load every record successfully.

The user confirmed that a pre-installation save loads with Marco Polo's DLL absent, while the affected test save did not. A crash captured with the DLL absent still contained the invalid form pointer `0xD821` during inventory initialization. That rules out an executing Marco Polo hook in that particular crash, but does not establish how the bad state originated.

The 0.1.2 game test used Oathvein, the Missives addon and the user's existing mod profile, with Container Item Distributor and the two main Narrative Loot entries disabled by the user. Single-item purchases and sales showed matching list prices, item-card quotes and gold transfers. A 15-iron purchase quoted 493 gold; the following transaction's starting balances confirmed exactly 493 gold transferred after native integer rounding was reconciled. A saved day-3 economy containing one merchant and 51,824 JSON bytes passed core validation and an exact persistence round-trip.

The first restart/load of that fresh save restored the economy record, then crashed while a RaceMenu equipment-dye task traversed a Whiterun guard's skeleton. The user confirmed that a second restart loaded the same save successfully; subsequent trades were logged. A preceding crash occurred during actor inventory reset and leveled-item generation. These failures remain unresolved. Their call stacks identify where invalid memory was used, not which component caused it. A live read-only check of 0.1.2 confirmed the ResetInventory jump enters Marco Polo and GetFireNode retains its original implementation. Successful co-save restoration alone is not a successful game-load test, and the successful retry is not a stability guarantee.

Subsequent 0.1.2 logs showed repeated book and clothing quotes with sufficient gold but no call to the original transaction handler. The chest-only stock check was rejecting these purchases. The native purchase helper at AE ID 13771 reads `TESDataHandler::merchantInventory` and follows `ExtraOriginalReference` handles, supporting both container stock and placed shop objects. Version 0.1.3 checks the selected sale row's count and tracks external stock separately. It does not mutate the temporary merchant inventory or replace the engine's item-transfer logic. Regression tests cover empty chests with display stock, mixed stacks after the chest is exhausted, exact-once accounting, unchanged displays during restocking, and loading older saves without the new stock field.

The user subsequently confirmed that 0.1.3 allowed purchases across the previously blocked inventory. Generic clutter such as cups, silverware and rugs has no default commodity mapping and retains vanilla pricing; being added by another mod does not itself exclude an item. The new character's day-3 save, containing two merchants and 71,928 economy JSON bytes, passed core validation and an exact persistence round-trip.

Two crashes while browsing the Markarth shop entered `BSXAudio2GameSound::Copy` at AE ID 68004 + 0x9C with an invalid attenuation-curve count, producing an 8 GiB copy request. A full restart with Marco Polo's DLL absent reproduced that same audio failure. DLL absence was checked in the loader log, live module mappings and crash report. Browsing the shop after a main-menu `coc`, without loading a save, succeeded with the DLL absent. This does not identify the source of the invalid audio data or establish that every item in the saved shop inventory was exercised.

Read-only audio sampling subsequently found the same invalid count, `0x40000000`, in a UI-focus sound identified by its complete resource ID as `sound/fx/ui/focus/ui_general_focus_01.wav`. The enabled loose-file provider was MEMOSPORE - UI Sound Effects. This observation also occurred in a successful fresh session, so the snapshot alone cannot establish causality. Temporarily disabling only that replacement WAV, with the vanilla archive fallback verified and Marco Polo's DLL still absent, allowed the user to browse the previously crashing saved-shop items. All 135 sampled objects in that comparison had a curve count of six. This is evidence for a sound-related workaround, not proof of the underlying defect or general stability. The original 0.1.3 DLL was restored for a further comparison with the vanilla sound.

With Marco Polo, CDF and SPID restored and the single replacement WAV still disabled, the user reported that browsing and purchases worked normally. The 13:57–14:00 log restored the same day-3 economy and recorded 12 priced purchases totaling 33 items, each with final player and merchant gold changes matching its quote. These included a mod-added book, clothing and eight goat horns supplied entirely by external stock. Stack settlement correctly transferred 241 gold for eight iron ingots and 374 for eight leather. No newer crash report appeared during this comparison. This passed the saved-shop browsing and purchase check; it did not retest exiting to the main menu, the original affected character or a further save/reload cycle. The one sound replacement remains disabled as a local workaround, with its original file preserved.

A separate, recurring exit-to-main-menu crash occurred during NPC inventory reset at `SkyrimSE.exe+0x1E0950`, dereferencing the invalid ownership pointer `0xE885`. It reproduced after a fresh main-menu `coc` with both Marco Polo and Container Distribution Framework absent, and again with CDF restored but SPID and Marco Polo absent. All three reports had the same fault and pointer despite naming different NPCs. CDF's frame in the preceding report was a call to the original inventory initializer, before its container-distribution rules ran. Removing either CDF or SPID did not resolve this exit failure, so both original DLLs were restored. Native inspection traced the bad pointer to ownership data copied from a calculated leveled item; its origin remains unresolved. These comparison tests did not overwrite saves, and neither failure is treated as proof of damage to the main save or economy co-save.

Run the CommonLib check in CI after configuring the Windows build:

```sh
python tools/audit-runtime.py --commonlib build/windows-release/_deps/commonlibsse-src
```

Add `--exe /path/to/SkyrimSE.exe --address-library /path/to/versionlib-1-6-1170-0.bin` for the local executable check. Game files are never included in artifacts.
