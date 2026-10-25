#pragma once
#include "core/Economy.h"
#include "core/Knowledge.h"
#include "core/Ledger.h"
#include <RE/Skyrim.h>
#include <SKSE/SKSE.h>
#include <atomic>
#include <memory>
#include <mutex>

namespace marco::runtime {
inline constexpr char DataPath[] = "Data/SKSE/Plugins/MarcoPolo/";
struct Snapshot : MarketReports {
    bool ready{};
    std::string status;
    json economy;
};
struct Context {
    RE::Actor *actor{};
    RE::TESObjectREFR *chest{};
    std::string merchant, market;
    explicit operator bool() const {
        return actor && chest && !market.empty();
    }
};
extern std::recursive_mutex mutex;
extern std::unique_ptr<Economy> economy;
extern Knowledge knowledge;
extern std::atomic_bool active;
extern std::atomic_uint64_t generation;
extern bool enabled, manageInventory, debugLogging;
extern thread_local bool internal;
bool Initialize();
void Start();
void Reset();
void Tick();
void Save(SKSE::SerializationInterface *);
void Load(SKSE::SerializationInterface *);
std::string Key(RE::TESForm *);
RE::TESForm *Resolve(const std::string &);
std::string Classify(RE::TESForm *);
std::string Region(RE::TESObjectREFR *);
std::string DateForDay(double day);
Context Vendor();
void SyncMerchant(const Context &, bool observeExternal = false);
void RestoreMerchant(const Context &);
Snapshot View();
void Publish();
bool InstallBarter();
bool RegisterMenu();
void RegisterLedger();
void ResetLedger();
int RawValue(const RE::InventoryEntryData *);
bool PlainItem(const RE::InventoryEntryData *);
void ExportState();
unsigned AdvanceEconomy(double now, unsigned budget);
void ObserveMarket(const std::string &market, const std::string &source);
bool ObserveSettlement();
void ResetInformation();
void RegisterInformation();
void SetRumorsEnabled(bool enabled);
bool RegisterPapyrus(RE::BSScript::IVirtualMachine *);
} // namespace marco::runtime
