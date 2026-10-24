#include "HookTargets.h"
#include "Runtime.h"
#include <MinHook.h>
#include <algorithm>
#include <cmath>
#include <cstring>

namespace marco::runtime {
namespace {
using AcceptFn = void (*)(RE::BarterMenu *, RE::FxDelegateHandler::CallbackProcessor *);
REL::Relocation<AcceptFn> acceptOriginal;
using AdvanceFn = void (*)(RE::BarterMenu *, float, std::uint32_t);
REL::Relocation<AdvanceFn> advanceOriginal;
unsigned attachAttempts{};
RE::FxDelegateHandler::CallbackFn *selectOriginal{};
using ResetFn = void (*)(RE::TESObjectREFR *, bool);
ResetFn resetOriginal{};
ResetFn resetVirtualOriginal{};
struct Guard {
    bool was = internal;
    Guard() {
        internal = true;
    }
    ~Guard() {
        internal = was;
    }
};
bool Ready(RE::BarterMenu *menu) {
    RE::GFxValue marker;
    return menu && menu->GetRuntimeData().root.GetMember("MarcoPoloQuantity", &marker) &&
           marker.IsBool() && marker.GetBool();
}
RE::InventoryEntryData *Selected(RE::BarterMenu *menu) {
    if (!menu)
        return nullptr;
    auto *list = menu->GetRuntimeData().itemList;
    auto *selected = list ? list->GetSelectedItem() : nullptr;
    return selected ? selected->data.objDesc : nullptr;
}
Quote QuoteFor(RE::BarterMenu *menu, const RE::InventoryEntryData *item, int count, bool buy) {
    auto c = Vendor();
    if (!c || !item || !active)
        return {};
    auto good = Classify(item->object);
    if (good.empty() || item->IsQuestObject())
        return {};
    RE::GFxValue mult;
    auto &root = menu->GetRuntimeData().root;
    if (!root.GetMember(buy ? "_buyMult" : "_sellMult", &mult) || !mult.IsNumber() ||
        !std::isfinite(mult.GetNumber()) || mult.GetNumber() <= 0)
        return {};
    double baseline =
        RawValue(item) * economy->Definitions().BarterMultiplier(mult.GetNumber(), buy);
    if (!std::isfinite(baseline) || baseline < 0 || baseline > 100000000)
        return {};
    return economy->Price(c.market, good, baseline, count, buy);
}
Quote CurrentQuote(RE::BarterMenu *menu, int count, bool buy) {
    return QuoteFor(menu, Selected(menu), count, buy);
}
int Count(RE::TESObjectREFR *ref, RE::TESBoundObject *f) {
    auto counts = ref->GetInventoryCounts([f](auto &obj) { return &obj == f; });
    auto it = counts.find(f);
    return it == counts.end() ? 0 : it->second;
}
void Gold(RE::TESObjectREFR *ref, Money delta) {
    auto *gold = RE::TESForm::LookupByID<RE::TESBoundObject>(0xF);
    if (delta > 0)
        ref->AddObjectToContainer(gold, nullptr, static_cast<int>(delta), nullptr);
    else if (delta < 0)
        ref->RemoveItem(gold, static_cast<int>(-delta), RE::ITEM_REMOVE_REASON::kRemove, nullptr,
                        nullptr);
}
void SetGoldState(RE::BarterMenu *menu, int playerGold, int merchantGold) {
    auto &data = menu->GetRuntimeData();
    data.playerGold = playerGold;
    data.merchantGold = merchantGold;
    data.root.SetMember("_playerGold", RE::GFxValue(data.playerGold));
    data.root.SetMember("_vendorGold", RE::GFxValue(data.merchantGold));
}
void RefreshGold(RE::BarterMenu *menu, const Context &context) {
    auto *gold = RE::TESForm::LookupByID<RE::TESBoundObject>(0xF);
    SetGoldState(menu, Count(RE::PlayerCharacter::GetSingleton(), gold),
                 Count(context.chest, gold));
    auto &data = menu->GetRuntimeData();
    RE::GFxValue bar;
    if (data.root.GetMember("bottomBar", &bar)) {
        RE::GFxValue values[]{RE::GFxValue(data.playerGold), RE::GFxValue(data.merchantGold)};
        bar.Invoke("updateBarterPriceInfo", nullptr, values, 2);
    }
}
void Select(const RE::FxDelegateArgs &args) {
    if (debugLogging)
        SKSE::log::info("ItemSelect: active={}, attached={}, arguments={}", active.load(),
                        Ready(static_cast<RE::BarterMenu *>(args.GetHandler())),
                        args.GetArgCount());
    if (!active || !Ready(static_cast<RE::BarterMenu *>(args.GetHandler())) ||
        args.GetArgCount() < 3 || !selectOriginal) {
        if (selectOriginal)
            selectOriginal(args);
        return;
    }
    auto *menu = static_cast<RE::BarterMenu *>(args.GetHandler());
    std::lock_guard lock(mutex);
    auto c = Vendor();
    auto *item = Selected(menu);
    if (!c || !item || !args[0].IsNumber() || !args[2].IsBool()) {
        if (debugLogging)
            SKSE::log::warn("Unquoted ItemSelect: vendor={}, selected={}, number={}, bool={}",
                            static_cast<bool>(c), item != nullptr, args[0].IsNumber(),
                            args[2].IsBool());
        selectOriginal(args);
        return;
    }
    double amount = args[0].GetNumber();
    if (!std::isfinite(amount) || amount < 1 || amount > 100000 || std::floor(amount) != amount)
        return;
    int n = static_cast<int>(amount);
    bool buy = args[2].GetBool();
    auto q = CurrentQuote(menu, n, buy);
    if (!q.quantity) {
        if (debugLogging)
            SKSE::log::info("Unpriced item: {} ({})", Key(item->object), item->object->GetName());
        selectOriginal(args);
        return;
    }
    auto *player = RE::PlayerCharacter::GetSingleton();
    auto *gold = RE::TESForm::LookupByID<RE::TESBoundObject>(0xF);
    auto *object = item->object;
    auto *selected = menu->GetRuntimeData().itemList->GetSelectedItem();
    RE::GFxValue rowCount;
    int offered = selected && selected->obj.GetMember("count", &rowCount) && rowCount.IsNumber() &&
                          std::isfinite(rowCount.GetNumber())
                      ? static_cast<int>(std::clamp(rowCount.GetNumber(), 0.0, 100000.0))
                      : 0;
    int chestBefore = Count(c.chest, object);
    Money playerGold = Count(player, gold), vendorGold = Count(c.chest, gold);
    if (debugLogging) {
        RE::GFxValue listValue, displayedGold;
        if (selected)
            selected->obj.GetMember("infoValue", &listValue);
        menu->GetRuntimeData().root.GetMember("_vendorGold", &displayedGold);
        SKSE::log::info("Quote: {} {} x {} ({}), total={}, UI unit={}, list unit={}, player "
                        "gold={}, vendor gold={}, offered={}, chest={}",
                        buy ? "BUY" : "SELL", n, Key(item->object), item->object->GetName(),
                        q.total, args[1].IsNumber() ? args[1].GetNumber() : -1,
                        listValue.IsNumber() ? listValue.GetNumber() : -1, playerGold, vendorGold,
                        offered, chestBefore);
        SKSE::log::info("Merchant funds: chest={}, native menu={}, UI={}", vendorGold,
                        menu->GetRuntimeData().merchantGold,
                        displayedGold.IsNumber() ? displayedGold.GetNumber() : -1);
    }
    // Native UI refreshes can use the temporary sale inventory's cached gold.
    // Keep the engine guard and visible balances on the coins we actually settle.
    SetGoldState(menu, static_cast<int>(playerGold), static_cast<int>(vendorGold));
    // A valid sale row can include objects outside the hidden merchant chest.
    // Check that row's available count; let Skyrim transfer from its sources.
    if (offered < n || q.total > (buy ? playerGold : vendorGold) || q.total > 100000000 ||
        playerGold + q.total > 1000000000 || vendorGold + q.total > 1000000000) {
        SKSE::log::warn("Transaction rejected: quantity={}, offered={}, total={}, payer gold={}", n,
                        offered, q.total, buy ? playerGold : vendorGold);
        RefreshGold(menu, c);
        if (!buy && q.total > vendorGold)
            RE::SendHUDMessage::ShowHUDMessage(std::format("{} has {} gold; this sale is worth {}.",
                                                           c.actor->GetName(), vendorGold, q.total)
                                                   .c_str());
        else
            RE::SendHUDMessage::ShowHUDMessage(
                "There is not enough stock or gold for this quantity.");
        return;
    }
    std::string key = Key(object), good = Classify(object);
    int before = Count(player, object);
    if (!economy->Get().merchants.contains(c.merchant))
        SyncMerchant(c, true);
    auto &m = economy->Edit().merchants.at(c.merchant);
    if (!m.items.contains(key))
        m.items[key] = {
            key, good, Count(c.chest, object), 0, 1.0, static_cast<double>(RawValue(item)), true};
    RE::GFxValue values[3]{args[0], RE::GFxValue(static_cast<double>(q.total) / n), args[2]};
    RE::FxDelegateArgs forwarded(RE::GFxValue(), args.GetHandler(), args.GetMovie(), values, 3);
    {
        Guard guard;
        selectOriginal(forwarded);
        int transferred = Count(player, object) - before;
        if (debugLogging)
            SKSE::log::info(
                "Engine transaction returned: items={}, player gold delta={}, vendor gold delta={}",
                transferred, Count(player, gold) - playerGold, Count(c.chest, gold) - vendorGold);
        if (transferred != (buy ? n : -n)) {
            SKSE::log::warn("Transaction declined or partial: expected {}, observed {}",
                            buy ? n : -n, transferred);
            SyncMerchant(c);
            return;
        }
        Money expectedPlayer = playerGold + (buy ? -q.total : q.total),
              expectedVendor = vendorGold + (buy ? q.total : -q.total);
        // The engine receives a unit-price number. Reconcile its integer rounding to
        // the exact displayed integral quote after a confirmed item transfer.
        Gold(player, expectedPlayer - Count(player, gold));
        Gold(c.chest, expectedVendor - Count(c.chest, gold));
    }
    auto &tracked = economy->Edit().merchants.at(c.merchant).items.at(key);
    int fromExternal = buy ? n - std::clamp(chestBefore - Count(c.chest, object), 0, n) : 0;
    bool protect = tracked.protectedItem;
    tracked.protectedItem = false;
    try {
        economy->Trade(c.merchant, key, n, buy, q.total, fromExternal);
    } catch (const std::exception &ex) {
        SKSE::log::error("Transaction accounting: {}", ex.what());
    }
    tracked.protectedItem = protect;
    SyncMerchant(c);
    RefreshGold(menu, c);
    Publish();
    if (debugLogging)
        SKSE::log::info("{} {} x {}: {} gold, market {}, external={}, player gold delta={}, vendor "
                        "gold delta={}",
                        buy ? "BUY" : "SELL", n, key, q.total, c.market, fromExternal,
                        Count(player, gold) - playerGold, Count(c.chest, gold) - vendorGold);
    auto gen = generation.load();
    SKSE::GetTaskInterface()->AddTask([gen] {
        if (!active || generation != gen)
            return;
        auto menu = RE::UI::GetSingleton()->GetMenu<RE::BarterMenu>();
        if (menu && menu->GetRuntimeData().itemList)
            menu->GetRuntimeData().itemList->Update();
    });
}
class Processor final : public RE::FxDelegateHandler::CallbackProcessor {
    RE::FxDelegateHandler::CallbackProcessor *downstream_;

  public:
    explicit Processor(RE::FxDelegateHandler::CallbackProcessor *p) : downstream_(p) {}
    void Process(const RE::GString &name, RE::FxDelegateHandler::CallbackFn *method) override {
        if (std::strcmp(name.c_str(), "ItemSelect") == 0) {
            selectOriginal = method;
            downstream_->Process(name, Select);
            if (debugLogging)
                SKSE::log::info("ItemSelect callback intercepted");
        } else
            downstream_->Process(name, method);
    }
};
void Accept(RE::BarterMenu *self, RE::FxDelegateHandler::CallbackProcessor *p) {
    Processor proxy(p);
    acceptOriginal(self, &proxy);
}
class QuantityHandler final : public RE::GFxFunctionHandler {
    RE::GFxValue original_;
    std::string method_;

  public:
    QuantityHandler(RE::GFxValue original, std::string method)
        : original_(original), method_(std::move(method)) {}
    void Call(Params &p) override {
        std::vector<RE::GFxValue> args;
        args.push_back(*p.thisPtr);
        for (unsigned i = 0; i < p.argCount; ++i)
            args.push_back(p.args[i]);
        auto menu = RE::UI::GetSingleton()->GetMenu<RE::BarterMenu>();
        if (active && Ready(menu.get()) && p.argCount) {
            std::lock_guard lock(mutex);
            RE::GFxValue amount;
            if (method_ == "UpdatePlayerInfo" && p.argCount >= 2) {
                if (auto c = Vendor()) {
                    auto *gold = RE::TESForm::LookupByID<RE::TESBoundObject>(0xF);
                    int playerGold = Count(RE::PlayerCharacter::GetSingleton(), gold);
                    int vendorGold = Count(c.chest, gold);
                    if (debugLogging && p.args[1].IsNumber() && p.args[1].GetNumber() != vendorGold)
                        SKSE::log::info("Corrected merchant balance refresh: cached={}, chest={}",
                                        p.args[1].GetNumber(), vendorGold);
                    SetGoldState(menu.get(), playerGold, vendorGold);
                    // These arguments are numbers, so replace the copied call
                    // arguments, preserving the merchant name and player stats.
                    args[1] = RE::GFxValue(playerGold);
                    args[2] = RE::GFxValue(vendorGold);
                }
            } else if (method_ == "UpdateItemCardInfo") {
                bool buy = menu->IsViewingVendorItems();
                auto q = CurrentQuote(menu.get(), 1, buy);
                RE::GFxValue mult;
                if (q.quantity && p.args[0].IsObject() &&
                    p.thisPtr->GetMember(buy ? "_buyMult" : "_sellMult", &mult) &&
                    mult.IsNumber() && mult.GetNumber() > 0)
                    p.args[0].SetMember(
                        "value", RE::GFxValue(static_cast<double>(q.total) / mult.GetNumber()));
            } else {
                bool has =
                    method_ == "doTransaction"
                        ? p.args[0].IsNumber()
                        : p.args[0].GetMember(
                              method_ == "onQuantitySliderChange" ? "value" : "amount", &amount);
                if (method_ == "doTransaction")
                    amount = p.args[0];
                if (has && amount.IsNumber() && std::isfinite(amount.GetNumber())) {
                    int n = static_cast<int>(std::clamp(amount.GetNumber(), 1.0, 100000.0));
                    auto q = CurrentQuote(menu.get(), n, menu->IsViewingVendorItems());
                    RE::GFxValue card, info;
                    if (q.quantity && p.thisPtr->GetMember("itemCard", &card) &&
                        card.GetMember("itemInfo", &info))
                        info.SetMember("value", RE::GFxValue(static_cast<double>(q.total) / n));
                }
            }
        }
        RE::GFxValue ignored;
        if (!original_.Invoke("call", p.retVal ? p.retVal : &ignored, args.data(),
                              static_cast<std::uint32_t>(args.size())))
            SKSE::log::error("Could not invoke SkyUI method {}", method_);
    }
};
// SkyUI processes these values before sorting and rendering. The final
// processor uses the same single-unit quote as the item card.
class ListPrices final : public RE::GFxFunctionHandler {
  public:
    void Call(Params &) override {
        auto menu = RE::UI::GetSingleton()->GetMenu<RE::BarterMenu>();
        if (!active || !Ready(menu.get()))
            return;
        std::lock_guard lock(mutex);
        auto *list = menu->GetRuntimeData().itemList;
        if (!list)
            return;
        for (auto *item : list->items) {
            if (!item || !item->data.objDesc)
                continue;
            RE::GFxValue filter;
            if (!item->obj.GetMember("filterFlag", &filter) || !filter.IsNumber())
                continue;
            bool buy = filter.GetNumber() >= 1024;
            auto q = QuoteFor(menu.get(), item->data.objDesc, 1, buy);
            if (!q.quantity)
                continue;
            item->obj.SetMember("infoValue", RE::GFxValue(static_cast<double>(q.total)));
            RE::GFxValue weight;
            if (item->obj.GetMember("infoWeight", &weight) && weight.IsNumber() &&
                weight.GetNumber() > 0)
                item->obj.SetMember("infoValueWeight",
                                    RE::GFxValue(std::floor(q.total / weight.GetNumber() + .5)));
        }
    }
};
bool WrapQuantity(RE::BarterMenu *menu) {
    if (Ready(menu))
        return true;
    // Never publish economy prices without the corresponding settlement hook.
    if (!selectOriginal)
        return false;
    auto &root = menu->GetRuntimeData().root;
    // SkyUI installs BarterDataSetter in its asynchronous setConfig callback.
    // Attaching earlier lets that later processor overwrite our list quotes
    // with vanilla prices while the item card/transaction uses economy prices.
    RE::GFxValue config, buyMult, sellMult;
    if (!root.GetMember("_config", &config) || !config.IsObject() ||
        !root.GetMember("_buyMult", &buyMult) || !buyMult.IsNumber() ||
        !root.GetMember("_sellMult", &sellMult) || !sellMult.IsNumber() ||
        !std::isfinite(buyMult.GetNumber()) || buyMult.GetNumber() <= 0 ||
        !std::isfinite(sellMult.GetNumber()) || sellMult.GetNumber() <= 0)
        return false;
    constexpr const char *methods[]{"UpdatePlayerInfo", "UpdateItemCardInfo",
                                    "onQuantitySliderChange", "onQuantityMenuSelect",
                                    "doTransaction"};
    RE::GFxValue originals[std::size(methods)], fn;
    auto *items = menu->GetRuntimeData().itemList;
    if (!items || !items->root.GetMember("addDataProcessor", &fn) || !fn.IsObject())
        return false;
    for (unsigned i = 0; i < std::size(methods); ++i)
        if (!root.GetMember(methods[i], &originals[i]) || !originals[i].IsObject())
            return false;
    SyncMerchant(Vendor(), true);
    for (unsigned i = 0; i < std::size(methods); ++i) {
        RE::GFxValue replacement;
        auto handler = RE::make_gptr<QuantityHandler>(originals[i], methods[i]);
        menu->uiMovie->CreateFunction(&replacement, handler.get());
        root.SetMember(methods[i], replacement);
    }
    RE::GFxValue processor, process;
    menu->uiMovie->CreateObject(&processor);
    auto handler = RE::make_gptr<ListPrices>();
    menu->uiMovie->CreateFunction(&process, handler.get());
    processor.SetMember("processList", process);
    if (!items->root.Invoke("addDataProcessor", nullptr, &processor, 1))
        return false;
    root.SetMember("MarcoPoloQuantity", RE::GFxValue(true));
    SKSE::log::info("SkyUI quotes attached; ItemSelect callback {}",
                    selectOriginal ? "captured" : "MISSING");
    if (debugLogging)
        SKSE::log::info("Barter multipliers: native buy={}, sell={}; Marco Polo buy={}, sell={}",
                        buyMult.GetNumber(), sellMult.GetNumber(),
                        economy->Definitions().BarterMultiplier(buyMult.GetNumber(), true),
                        economy->Definitions().BarterMultiplier(sellMult.GetNumber(), false));
    return true;
}
void AdvanceMovie(RE::BarterMenu *menu, float interval, std::uint32_t time) {
    advanceOriginal(menu, interval, time);
    if (!active || Ready(menu))
        return;
    std::lock_guard lock(mutex);
    constexpr unsigned maxAttachAttempts = 300;
    if (attachAttempts++ >= maxAttachAttempts)
        return;
    if (WrapQuantity(menu)) {
        SKSE::log::info("SkyUI quote integration attached");
        if (auto c = Vendor())
            RefreshGold(menu, c);
        if (menu->GetRuntimeData().itemList)
            menu->GetRuntimeData().itemList->Update();
    } else if (attachAttempts == maxAttachAttempts) {
        SKSE::log::warn("Unsupported barter menu; prices left to the game");
        RE::SendHUDMessage::ShowHUDMessage(
            "Marco Polo needs a compatible SkyUI barter menu for prices.");
    }
}
bool SuppressReset(RE::TESObjectREFR *ref) {
    if (!active || !manageInventory)
        return false;
    std::lock_guard lock(mutex);
    return active && economy && economy->Get().merchants.contains(Key(ref));
}
void ResetInventory(RE::TESObjectREFR *ref, bool leveled) {
    if (SuppressReset(ref))
        return;
    // Do not hold the economy lock through engine work or another mod's callbacks.
    resetOriginal(ref, leveled);
}
void ResetInventoryVirtual(RE::TESObjectREFR *ref, bool leveled) {
    if (!SuppressReset(ref))
        resetVirtualOriginal(ref, leveled);
}
class Events final : public RE::BSTEventSink<RE::MenuOpenCloseEvent>,
                     public RE::BSTEventSink<RE::TESContainerChangedEvent> {
  public:
    RE::BSEventNotifyControl ProcessEvent(const RE::MenuOpenCloseEvent *e,
                                          RE::BSTEventSource<RE::MenuOpenCloseEvent> *) override {
        if (e && active && e->menuName == RE::BarterMenu::MENU_NAME && e->opening) {
            std::lock_guard lock(mutex);
            attachAttempts = 0;
            auto c = Vendor();
            if (c) {
                double now = RE::Calendar::GetSingleton()->GetDaysPassed();
                economy->Advance(now, 32);
                if (economy->Get().day + 1 <= now) {
                    RE::UIMessageQueue::GetSingleton()->AddMessage(
                        RE::BarterMenu::MENU_NAME, RE::UI_MESSAGE_TYPE::kHide, nullptr);
                    RE::SendHUDMessage::ShowHUDMessage("Marco Polo is catching up after a long "
                                                       "absence. Try trading again shortly.");
                    return RE::BSEventNotifyControl::kContinue;
                }
                RestoreMerchant(c);
                SyncMerchant(c, true);
                auto menu = RE::UI::GetSingleton()->GetMenu<RE::BarterMenu>();
                if (menu) {
                    WrapQuantity(menu.get());
                    RefreshGold(menu.get(), c);
                    if (menu->GetRuntimeData().itemList)
                        menu->GetRuntimeData().itemList->Update();
                }
                Publish();
            }
        }
        return RE::BSEventNotifyControl::kContinue;
    }
    RE::BSEventNotifyControl
    ProcessEvent(const RE::TESContainerChangedEvent *e,
                 RE::BSTEventSource<RE::TESContainerChangedEvent> *) override {
        if (!e || !active || internal || !economy || e->itemCount <= 0)
            return RE::BSEventNotifyControl::kContinue;
        std::lock_guard lock(mutex);
        for (auto [id, sign] : {std::pair{e->oldContainer, -1}, std::pair{e->newContainer, 1}}) {
            auto *ref = RE::TESForm::LookupByID<RE::TESObjectREFR>(id);
            auto it = economy->Get().merchants.find(Key(ref));
            if (it == economy->Get().merchants.end())
                continue;
            Merchant updated = it->second;
            if (e->baseObj == 0xF) {
                updated.cash = std::clamp<Money>(
                    updated.cash + sign * static_cast<Money>(e->itemCount), 0, 1000000000);
                economy->RegisterMerchant(std::move(updated));
                continue;
            }
            auto key = Key(RE::TESForm::LookupByID(e->baseObj));
            auto item = updated.items.find(key);
            if (item != updated.items.end()) {
                item->second.quantity = static_cast<int>(std::clamp<std::int64_t>(
                    item->second.quantity + sign * static_cast<std::int64_t>(e->itemCount), 0,
                    100000));
                economy->RegisterMerchant(std::move(updated));
            }
        }
        return RE::BSEventNotifyControl::kContinue;
    }
};
Events events;
} // namespace
int RawValue(const RE::InventoryEntryData *item) {
    return item ? item->GetValue() : 0;
}
bool InstallBarter() {
    auto init = MH_Initialize();
    if (init != MH_OK && init != MH_ERROR_ALREADY_INITIALIZED)
        return false;
    REL::Relocation<std::uintptr_t> refTable{RE::VTABLE_TESObjectREFR[0]};
    if (manageInventory) {
        // Detour the independently verified engine ID, never whatever code a
        // vtable slot currently points at. CID installs a virtual override here.
        const REL::RelocationID resetID{hooks::ResetInventorySE, hooks::ResetInventoryAE};
        REL::Relocation<std::uintptr_t> resetAddress{resetID};
        auto *reset = reinterpret_cast<void *>(resetAddress.address());
        if (MH_CreateHook(reset, reinterpret_cast<void *>(ResetInventory),
                          reinterpret_cast<void **>(&resetOriginal)) != MH_OK ||
            MH_EnableHook(reset) != MH_OK) {
            enabled = false;
            SKSE::log::error("Merchant reset hook failed; economy disabled");
            return false;
        }
        auto current = reinterpret_cast<ResetFn *>(refTable.address())[hooks::ResetInventorySlot];
        if (reinterpret_cast<std::uintptr_t>(current) != resetAddress.address()) {
            // Preserve the entire existing virtual chain. For managed shops,
            // suppress both the reset and any distributor's post-reset reroll.
            resetVirtualOriginal = reinterpret_cast<ResetFn>(
                refTable.write_vfunc(hooks::ResetInventorySlot, ResetInventoryVirtual));
            SKSE::log::info("ResetInventory: engine ID {}, existing virtual hook chained ({:x})",
                            resetID.id(), reinterpret_cast<std::uintptr_t>(current));
        } else {
            SKSE::log::info("ResetInventory: engine ID {}, original virtual entry", resetID.id());
        }
    }
    REL::Relocation<std::uintptr_t> table{RE::VTABLE_BarterMenu[0]};
    acceptOriginal = table.write_vfunc(hooks::BarterAcceptSlot, Accept);
    advanceOriginal = table.write_vfunc(hooks::BarterAdvanceSlot, AdvanceMovie);
    RE::UI::GetSingleton()->AddEventSink<RE::MenuOpenCloseEvent>(&events);
    RE::ScriptEventSourceHolder::GetSingleton()->AddEventSink<RE::TESContainerChangedEvent>(
        &events);
    SKSE::log::info(
        "Native barter quotes, transaction settlement and merchant reset integration installed");
    return true;
}
} // namespace marco::runtime
