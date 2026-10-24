#include "Runtime.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <format>
#include <fstream>
#include <thread>

namespace marco::runtime {
std::recursive_mutex mutex;
std::unique_ptr<Economy> economy;
std::atomic_bool active{false};
std::atomic_uint64_t generation{0};
bool enabled = true, manageInventory = true, debugLogging = false;
thread_local bool internal = false;
namespace {
json classification, settings;
std::map<RE::FormID, std::string> cached, locations;
std::mutex viewMutex;
Snapshot snapshot;
std::jthread ticker;
bool loaded = false, loadFailed = false;
std::string status = "Starting Marco Polo";
double Now() {
    auto *c = RE::Calendar::GetSingleton();
    return c ? c->GetDaysPassed() : 0;
}
struct InternalScope {
    bool old = internal;
    InternalScope() {
        internal = true;
    }
    ~InternalScope() {
        internal = old;
    }
};
} // namespace
std::string Key(RE::TESForm *f) {
    if (!f || f->IsDynamicForm())
        return {};
    auto *file = f->GetFile(0);
    if (!file)
        return {};
    return std::format("{}|{:06X}", file->fileName, f->GetLocalFormID());
}
RE::TESForm *Resolve(const std::string &key) {
    try {
        auto p = key.rfind('|');
        if (p == std::string::npos)
            return nullptr;
        return RE::TESDataHandler::GetSingleton()->LookupForm(
            static_cast<RE::FormID>(std::stoul(key.substr(p + 1), nullptr, 16)), key.substr(0, p));
    } catch (...) {
        return nullptr;
    }
}
std::string Classify(RE::TESForm *f) {
    if (!f || f->IsGold() || f->IsLockpick() || f->IsDynamicForm())
        return {};
    auto it = cached.find(f->GetFormID());
    if (it != cached.end())
        return it->second;
    std::string answer, key = Key(f);
    if (classification.at("forms").contains(key))
        answer = classification["forms"][key];
    else if (auto *keywords = f->As<RE::BGSKeywordForm>()) {
        std::size_t specificity = 0;
        for (auto &[kw, good] : classification.at("keywords").items())
            if (kw.size() > specificity && keywords->HasKeywordString(kw)) {
                answer = good;
                specificity = kw.size();
            }
    }
    if (answer.empty()) {
        const char *type = "";
        switch (f->GetFormType()) {
        case RE::FormType::Weapon:
            type = "Weapon";
            break;
        case RE::FormType::Armor:
            type = "Armor";
            break;
        case RE::FormType::Ingredient:
            type = "Ingredient";
            break;
        case RE::FormType::AlchemyItem:
            type = "AlchemyItem";
            break;
        case RE::FormType::SoulGem:
            type = "SoulGem";
            break;
        case RE::FormType::Book:
            type = "Book";
            break;
        case RE::FormType::Scroll:
            type = "Scroll";
            break;
        default:
            break;
        }
        if (classification.at("types").contains(type))
            answer = classification["types"][type];
    }
    for (auto &excluded : classification.at("excluded"))
        if (excluded == key)
            answer.clear();
    if (!answer.empty())
        economy->Definitions().Good(answer);
    cached[f->GetFormID()] = answer;
    return answer;
}
std::string Region(RE::TESObjectREFR *ref) {
    if (!ref)
        return {};
    auto *loc = ref->GetCurrentLocation();
    unsigned depth = 0;
    while (loc && depth++ < 64) {
        auto it = locations.find(loc->GetFormID());
        if (it != locations.end())
            return it->second;
        loc = loc->parentLoc;
    }
    // Solstheim's worldspace is more reliable than individual DLC location trees.
    if (auto *world = ref->GetWorldspace(); world && Key(world) == "Dragonborn.esm|000800")
        return "solstheim";
    return {};
}
std::string DateForDay(double target) {
    auto *calendar = RE::Calendar::GetSingleton();
    if (!calendar)
        return {};
    constexpr const char *months[]{"Morning Star", "Sun's Dawn", "First Seed",   "Rain's Hand",
                                   "Second Seed",  "Midyear",    "Sun's Height", "Last Seed",
                                   "Hearthfire",   "Frostfall",  "Sun's Dusk",   "Evening Star"};
    int month = static_cast<int>(calendar->GetMonth() % 12),
        year = static_cast<int>(calendar->GetYear());
    int delta =
        static_cast<int>(std::clamp(std::floor(target) - std::floor(Now()), -365000.0, 365000.0));
    year += delta / 365;
    delta %= 365;
    int day = static_cast<int>(calendar->GetDay()) + delta;
    while (day > RE::Calendar::DAYS_IN_MONTH[month]) {
        day -= RE::Calendar::DAYS_IN_MONTH[month];
        if (++month == 12) {
            month = 0;
            ++year;
        }
    }
    while (day < 1) {
        if (--month < 0) {
            month = 11;
            --year;
        }
        day += RE::Calendar::DAYS_IN_MONTH[month];
    }
    return std::format("{} {}, 4E {}", day, months[month], year);
}
Context Vendor() {
    Context c;
    auto handle = RE::BarterMenu::GetTargetRefHandle();
    RE::NiPointer<RE::TESObjectREFR> ref;
    RE::LookupReferenceByHandle(handle, ref);
    if (!ref)
        return c;
    c.actor = ref->As<RE::Actor>();
    if (!c.actor)
        return c;
    auto *f = c.actor->GetVendorFaction();
    if (!f)
        return c;
    c.chest = f->vendorData.merchantContainer;
    if (!c.chest)
        return c;
    c.merchant = Key(c.chest);
    c.market = Region(c.actor);
    if (c.market.empty())
        c.market = Region(RE::PlayerCharacter::GetSingleton());
    return c;
}
bool PlainItem(const RE::InventoryEntryData *entry) {
    if (!entry || entry->IsQuestObject() || entry->IsEnchanted())
        return false;
    if (entry->extraLists)
        for (auto *list : *entry->extraLists)
            if (list)
                for (auto &data : *list) {
                    switch (data.GetType()) {
                    case RE::ExtraDataType::kCount:
                    case RE::ExtraDataType::kOwnership:
                    case RE::ExtraDataType::kLeveledItem:
                    case RE::ExtraDataType::kLeveledItemBase:
                        break;
                    default:
                        return false;
                    }
                }
    return true;
}
void SyncMerchant(const Context &c, bool observeExternal) {
    if (!c || c.merchant.empty() || !economy)
        return;
    InternalScope guard;
    auto it = economy->Get().merchants.find(c.merchant);
    Merchant m = it == economy->Get().merchants.end() ? Merchant{} : it->second;
    m.id = c.merchant;
    m.market = c.market;
    m.lastVisit = Now();
    m.cash = c.chest->GetInventoryCounts([](RE::TESBoundObject &f) {
        return f.IsGold();
    })[RE::TESForm::LookupByID<RE::TESBoundObject>(0xF)];
    auto inventory = c.chest->GetInventory();
    std::set<std::string> seen;
    for (auto &[form, pair] : inventory) {
        auto &[count, entry] = pair;
        auto good = Classify(form);
        auto key = Key(form);
        if (good.empty() || key.empty() || count < 0 || !entry)
            continue;
        seen.insert(key);
        bool protect = !PlainItem(entry.get());
        auto old = m.items.find(key);
        int target = old == m.items.end() ? std::clamp(count, 0, 1000) : old->second.target;
        int external = old == m.items.end() ? 0 : old->second.externalQuantity;
        m.items[key] = {key,    good, count,
                        target, 1.0,  static_cast<double>(std::max(1, RawValue(entry.get()))),
                        protect, external};
    }
    for (auto &[key, item] : m.items)
        if (!seen.contains(key))
            item.quantity = 0;
    if (observeExternal) {
        for (auto &[key, item] : m.items)
            item.externalQuantity = 0;
        // Skyrim's sale inventory also contains shop displays and goods in
        // other containers. Its temporary extra lists identify each source
        // with ExtraOriginalReference; the native purchase callback uses the
        // same handles to remove the actual object. Never write to this cache.
        auto *offers = RE::TESDataHandler::GetSingleton()->GetMerchantInventory();
        if (offers && offers->entryList)
            for (auto *entry : *offers->entryList) {
                if (!entry || !entry->object || !entry->extraLists)
                    continue;
                auto key = Key(entry->object), good = Classify(entry->object);
                if (key.empty() || good.empty())
                    continue;
                int external = 0;
                for (auto *list : *entry->extraLists) {
                    if (!list)
                        continue;
                    auto *original = list->GetByType<RE::ExtraOriginalReference>();
                    auto source = original ? original->reference.get()
                                           : RE::NiPointer<RE::TESObjectREFR>{};
                    if (source && source.get() != c.chest)
                        external += std::clamp(list->GetCount(), 0, 100000 - external);
                }
                if (!external)
                    continue;
                auto [it, inserted] = m.items.try_emplace(
                    key, Item{key, good, 0, 0, 1.0,
                              static_cast<double>(std::max(1, RawValue(entry))), true});
                it->second.externalQuantity =
                    std::min(100000, it->second.externalQuantity + external);
            }
    }
    economy->RegisterMerchant(std::move(m));
}
void RestoreMerchant(const Context &c) {
    if (!manageInventory || !c || !economy)
        return;
    auto it = economy->Get().merchants.find(c.merchant);
    if (it == economy->Get().merchants.end())
        return;
    InternalScope guard;
    auto inventory = c.chest->GetInventory();
    for (auto &[key, item] : it->second.items) {
        if (item.protectedItem)
            continue;
        auto *form = Resolve(key);
        auto *bound = form ? form->As<RE::TESBoundObject>() : nullptr;
        if (!bound)
            continue;
        auto actual = inventory.find(bound);
        int count = actual == inventory.end() ? 0 : actual->second.first;
        if (actual != inventory.end() && !PlainItem(actual->second.second.get()))
            continue;
        int delta = item.quantity - count;
        if (delta > 0)
            c.chest->AddObjectToContainer(bound, nullptr, delta, nullptr);
        else if (delta < 0)
            c.chest->RemoveItem(bound, -delta, RE::ITEM_REMOVE_REASON::kRemove, nullptr, nullptr);
    }
    // Gold is settled by actual transactions; background replenishment uses the ledger cash.
    auto *gold = RE::TESForm::LookupByID<RE::TESBoundObject>(0xF);
    auto actual = c.chest->GetInventoryCounts([&](auto &f) { return &f == gold; });
    Money delta = it->second.cash - actual[gold];
    if (delta > 0 && delta < 1000000)
        c.chest->AddObjectToContainer(gold, nullptr, static_cast<int>(delta), nullptr);
    else if (delta < 0 && delta > -1000000)
        c.chest->RemoveItem(gold, static_cast<int>(-delta), RE::ITEM_REMOVE_REASON::kRemove,
                            nullptr, nullptr);
}
bool Initialize() {
    std::lock_guard lock(mutex);
    try {
        std::ifstream f(std::string(DataPath) + "economy.json");
        economy = std::make_unique<Economy>(Config::Parse(json::parse(f)));
        std::ifstream c(std::string(DataPath) + "classification.json");
        classification = json::parse(c);
        for (const char *table : {"forms", "keywords", "types"}) {
            if (!classification.at(table).is_object())
                throw std::runtime_error("Invalid classification table");
            for (auto &[key, value] : classification.at(table).items())
                economy->Definitions().Good(value.get<std::string>());
        }
        if (!classification.at("excluded").is_array())
            throw std::runtime_error("Invalid excluded forms");
        std::ifstream opts(std::string(DataPath) + "settings.json");
        settings = opts ? json::parse(opts) : json::object();
        enabled = settings.value("enabled", true);
        manageInventory = settings.value("manageInventory", true);
        debugLogging = settings.value("debugLogging", false);
        for (auto &m : economy->Definitions().markets)
            if (auto *form = Resolve(m.location))
                locations[form->GetFormID()] = m.id;
        ticker = std::jthread([](std::stop_token stop) {
            while (!stop.stop_requested()) {
                std::this_thread::sleep_for(std::chrono::seconds(2));
                if (active) {
                    auto gen = generation.load();
                    SKSE::GetTaskInterface()->AddTask([gen] {
                        if (active && generation == gen)
                            try {
                                Tick();
                            } catch (const std::exception &e) {
                                active = false;
                                SKSE::log::error("Economy stopped: {}", e.what());
                            }
                    });
                }
            }
        });
        status = "Economy ready";
        return true;
    } catch (const std::exception &ex) {
        status = ex.what();
        SKSE::log::error("Configuration: {}", ex.what());
        return false;
    }
}
void Reset() {
    active = false;
    ++generation;
    std::lock_guard lock(mutex);
    loaded = false;
    loadFailed = false;
    ResetMissives();
    if (economy)
        economy->NewGame(0, 1);
    {
        std::lock_guard v(viewMutex);
        snapshot = {};
    }
}
void Start() {
    std::lock_guard lock(mutex);
    if (!economy || loadFailed) {
        Publish();
        return;
    }
    if (!loaded)
        economy->NewGame(Now(), static_cast<std::uint64_t>(
                                    std::chrono::steady_clock::now().time_since_epoch().count()));
    loaded = true;
    active = enabled;
    UpdateBoards(true);
    Publish();
}
void Tick() {
    std::lock_guard lock(mutex);
    if (!economy || !active)
        return;
    auto *ui = RE::UI::GetSingleton();
    if (!ui || ui->GameIsPaused())
        return;
    // CWObj has stages 1 and 255. It is not a continuous progress counter.
    auto *civil = RE::TESForm::LookupByID<RE::TESQuest>(0x29940);
    double war = civil && !civil->IsCompleted() && civil->currentStage < 255 ? 0.5 : 0;
    for (auto &m : economy->Definitions().markets)
        economy->SetConditions(m.id, war,
                               war * (m.id == "eastmarch" || m.id == "haafingar" ? .3 : .1));
    unsigned steps = economy->Advance(Now(), 8);
    if (steps) {
        UpdateBoards();
        if (debugLogging)
            SKSE::log::info("Economy day {}, {} steps, {} shipments", economy->Get().day, steps,
                            economy->Get().shipments.size());
    }
    if (steps)
        Publish();
}
void Publish() {
    Snapshot v;
    v.ready = active && economy;
    v.status = status;
    if (v.ready) {
        v.market = Region(RE::PlayerCharacter::GetSingleton());
        v.economy = economy->Save();
        v.economy["date"] = DateForDay(economy->Get().day);
        for (auto &market : economy->Definitions().markets)
            v.economy["marketNames"][market.id] = market.name;
        for (auto &good : economy->Definitions().commodities)
            v.economy["commodityNames"][good.id] = good.name;
        if (!v.market.empty())
            v.reports = economy->Reports(v.market);
    }
    std::lock_guard lock(viewMutex);
    snapshot = std::move(v);
}
Snapshot View() {
    std::lock_guard lock(viewMutex);
    return snapshot;
}
void Save(SKSE::SerializationInterface *s) {
    std::lock_guard lock(mutex);
    if (!economy || !loaded || loadFailed)
        return;
    try {
        auto data = economy->Save().dump();
        if (!s->OpenRecord(0x45434F4E, 1) ||
            !s->WriteRecordData(data.data(), static_cast<std::uint32_t>(data.size())))
            SKSE::log::error("Could not write economy co-save");
        else if (debugLogging)
            SKSE::log::info("Saved economy: day {}, {} bytes", economy->Get().day, data.size());
    } catch (const std::exception &ex) {
        SKSE::log::error("Save: {}", ex.what());
    }
}
void Load(SKSE::SerializationInterface *s) {
    std::lock_guard lock(mutex);
    if (!economy)
        return;
    std::uint32_t type, version, length;
    while (s->GetNextRecordInfo(type, version, length)) {
        if (type != 0x45434F4E)
            continue;
        try {
            if (version != 1 || length > 32 * 1024 * 1024)
                throw std::runtime_error("Unsupported or oversized economy save");
            std::string data(length, '\0');
            if (s->ReadRecordData(data.data(), length) != length)
                throw std::runtime_error("Truncated economy save");
            economy->Load(json::parse(data));
            loaded = true;
            SKSE::log::info("Loaded economy: day {}, {} merchants, {} bytes", economy->Get().day,
                            economy->Get().merchants.size(), length);
        } catch (const std::exception &ex) {
            loadFailed = true;
            active = false;
            status = "Economy disabled: incompatible save data";
            SKSE::log::error("Load: {}", ex.what());
        }
    }
}
void ExportState() {
    std::lock_guard lock(mutex);
    if (!economy)
        return;
    auto p = SKSE::log::log_directory();
    if (p) {
        *p /= "MarcoPolo-state.json";
        std::ofstream f(*p);
        f << economy->Save().dump(2);
        RE::SendHUDMessage::ShowHUDMessage(
            "Marco Polo: diagnostic state exported beside SKSE logs.");
    }
}
} // namespace marco::runtime
