#include "HookTargets.h"
#include "NoticeUI.h"
#include "Runtime.h"
#include <windows.h>

#include "QuickLootAPI.h"
#include "core/Notices.h"
#include <MinHook.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <format>
#include <optional>

namespace marco::runtime {
namespace {
constexpr char Addon[] = "Marco Polo - Missives.esp";
struct Board {
    const char *market;
    RE::FormID reference;
};
constexpr std::array<Board, 9> boards{{{"whiterun", 0x12CC},
                                       {"eastmarch", 0x9477},
                                       {"haafingar", 0x948F},
                                       {"rift", 0x9491},
                                       {"reach", 0x94A2},
                                       {"falkreath", 0x94A6},
                                       {"hjaalmarch", 0x94AA},
                                       {"pale", 0x94AE},
                                       {"winterhold", 0x94B2}}};
// Only concrete, ordinary goods are eligible for contracts. Reward valuation
// comes from that exact form, rather than a generic commodity's average value.
constexpr std::array<std::pair<const char *, RE::FormID>, 15> delivery{{{"grain", 0x4B0BA},
                                                                        {"meat", 0x65C9B},
                                                                        {"drink", 0x34C5E},
                                                                        {"timber", 0x6F993},
                                                                        {"leather", 0xDB5D2},
                                                                        {"iron", 0x5ACE4},
                                                                        {"steel", 0x5ACE5},
                                                                        {"silver", 0x5ACE3},
                                                                        {"gold", 0x5AD9E},
                                                                        {"corundum", 0x5AD93},
                                                                        {"dwarven", 0xDB8A2},
                                                                        {"orichalcum", 0x5AD99},
                                                                        {"moonstone", 0x5AD9F},
                                                                        {"quicksilver", 0x5ADA0},
                                                                        {"ebony", 0x5AD9D}}};
bool installed = false;
bool paperCleanupQueued = false, papersDirty = false, receiptGrantPending = false;
std::uint64_t readSequence{};
enum class Paper { bulletin, posting, receipt };
struct Reading {
    unsigned region{};
    Paper paper{};
    std::uint64_t contract{}, generation{}, sequence{};
    bool opened{};
};
std::optional<Reading> reading;
std::map<RE::TESDescription *, std::pair<unsigned, Paper>> descriptions;
using DescriptionFn = void (*)(RE::TESDescription *, RE::BSString &, RE::TESForm *, std::uint32_t);
DescriptionFn descriptionOriginal{};
using OpenBookFn = void (*)(const RE::BSString &, const RE::ExtraDataList *, RE::TESObjectREFR *,
                            RE::TESObjectBOOK *, const RE::NiPoint3 &, const RE::NiMatrix3 &, float,
                            bool);
OpenBookFn openBookOriginal{};
using AcceptFn = void (*)(RE::ContainerMenu *, RE::FxDelegateHandler::CallbackProcessor *);
REL::Relocation<AcceptFn> containerAcceptOriginal;
using AdvanceFn = void (*)(RE::ContainerMenu *, float, std::uint32_t);
REL::Relocation<AdvanceFn> containerAdvanceOriginal;
RE::FxDelegateHandler::CallbackFn *transferOriginal{}, *equipOriginal{}, *takeAllOriginal{},
    *disabledOriginal{};
RE::TESObjectBOOK *Note(unsigned region, Paper paper) {
    return RE::TESDataHandler::GetSingleton()->LookupForm<RE::TESObjectBOOK>(
        paper == Paper::posting ? 0x812 + region
                                : 0x800 + region * 2 + (paper == Paper::receipt ? 1 : 0),
        Addon);
}
RE::TESObjectREFR *BoardRef(unsigned region) {
    return RE::TESDataHandler::GetSingleton()->LookupForm<RE::TESObjectREFR>(
        boards.at(region).reference, "Missives.esp");
}
bool Nearby(unsigned region) {
    auto *p = RE::PlayerCharacter::GetSingleton();
    auto *b = BoardRef(region);
    return p && b &&
           (p->GetParentCell() == b->GetParentCell() ||
            (p->GetWorldspace() && p->GetWorldspace() == b->GetWorldspace())) &&
           (p->GetPosition() - b->GetPosition()).Length() < 1200.0f;
}
const Contract *Current(unsigned region) {
    for (auto &c : economy->Get().contracts)
        if (c.market == boards.at(region).market &&
            (c.status == ContractStatus::offered || c.status == ContractStatus::accepted))
            return &c;
    return nullptr;
}
std::optional<std::pair<unsigned, Paper>> Identify(RE::TESForm *form) {
    if (!form || form->IsDynamicForm())
        return std::nullopt;
    auto id = form->GetLocalFormID();
    if (id < 0x800 || id >= 0x81B)
        return std::nullopt;
    unsigned region = id < 0x812 ? (id - 0x800) / 2 : id - 0x812;
    auto kind = id >= 0x812 ? Paper::posting : id % 2 ? Paper::receipt : Paper::bulletin;
    if (form == Note(region, kind))
        return {{region, kind}};
    return std::nullopt;
}
bool AtBoardContainer() {
    auto target = RE::TESObjectREFR::LookupByHandle(RE::ContainerMenu::GetTargetRefHandle());
    if (target)
        for (unsigned i = 0; i < boards.size(); ++i)
            if (target.get() == BoardRef(i))
                return true;
    return false;
}
std::string Terms(const Contract &c) {
    auto hold = economy->Definitions().Market(c.market).name;
    return std::format("Goods wanted\n\n{}: {}\nPayment: {} septims\nDeliver by {}\n"
                       "Delivery: {} noticeboard\n\n— The merchants of {}",
                       c.itemName, c.quantity, c.reward, DateForDay(c.deadline), hold, hold);
}
std::string ReceiptName(unsigned region, std::uint64_t id) {
    return OrderPaperName(Note(region, Paper::receipt)->GetName(), id);
}
std::uint64_t ReceiptID(unsigned region, const RE::ExtraDataList *extra) {
    auto *display = extra ? extra->GetByType<RE::ExtraTextDisplayData>() : nullptr;
    if (!display || !display->displayName.c_str())
        return 0;
    return OrderPaperID(Note(region, Paper::receipt)->GetName(), display->displayName.c_str());
}
std::uint64_t ReceiptStackID(unsigned region, const RE::InventoryEntryData *entry, int count) {
    std::vector<std::pair<std::uint64_t, int>> copies;
    if (entry && entry->extraLists)
        for (auto *extra : *entry->extraLists)
            if (extra)
                copies.emplace_back(ReceiptID(region, extra), extra->GetCount());
    return OrderPaperStackID(count, copies);
}
RE::ItemList::Item *SelectedItem(RE::ItemList *list) {
    // CommonLib's GetSelectedItem calls root.GetMember internally.
    return list && list->root.IsObject() ? list->GetSelectedItem() : nullptr;
}
struct PaperIdentity {
    std::uint64_t id{};
    const char *source{"unresolved"};
};
PaperIdentity ResolveReceipt(unsigned region, const RE::ExtraDataList *extra,
                             RE::TESObjectREFR *reference, RE::TESObjectBOOK *book) {
    // An explicit physical instance wins, even when untagged or obsolete.
    // Never replace its identity with the current order or another paper.
    if (extra)
        return {ReceiptID(region, extra), "book extra"};
    if (reference)
        return {ReceiptID(region, &reference->extraList), "world reference"};
    auto *ui = RE::UI::GetSingleton();
    if (!ui)
        return {};
    RE::ItemList::Item *selected{};
    bool inventory = false;
    if (auto menu = ui->GetMenu<RE::InventoryMenu>();
        menu && ui->IsMenuOpen(RE::InventoryMenu::MENU_NAME)) {
        selected = SelectedItem(menu->GetRuntimeData().itemList);
        inventory = true;
    } else if (auto menu = ui->GetMenu<RE::ContainerMenu>();
               menu && ui->IsMenuOpen(RE::ContainerMenu::MENU_NAME)) {
        selected = SelectedItem(menu->GetRuntimeData().itemList);
    }
    auto *entry = selected ? selected->data.objDesc : nullptr;
    if (!entry || entry->object != book)
        return {};
    if (entry->extraLists)
        for (auto *instance : *entry->extraLists)
            if (instance)
                return {ReceiptStackID(region, entry, entry->countDelta), "selected stack"};
    // Some menu lists contain base-form-only entries. A single carried copy
    // can resolve that omission, but never guess between copies or owners.
    auto *player = RE::PlayerCharacter::GetSingleton();
    if (inventory && player) {
        auto carried = player->GetInventory([book](auto &form) { return &form == book; });
        auto found = carried.find(book);
        if (found != carried.end() && found->second.first == 1)
            return {ReceiptStackID(region, found->second.second.get(), 1), "single carried copy"};
    }
    return {};
}
std::string Text(unsigned region, Paper paper) {
    if (paper != Paper::bulletin) {
        if (auto *c = Current(region); c && paper == Paper::posting)
            return Terms(*c);
        return "This order is closed.";
    }
    std::string out = std::format("Trade in {}\n{}\n\n",
                                  economy->Definitions().Market(boards.at(region).market).name,
                                  DateForDay(economy->Get().day));
    auto reports = economy->Reports(boards.at(region).market);
    std::stable_sort(reports.begin(), reports.end(), [](auto &a, auto &b) {
        return std::abs(a.multiplier - 1) > std::abs(b.multiplier - 1);
    });
    for (unsigned i = 0; i < std::min<std::size_t>(4, reports.size()); ++i) {
        auto name = economy->Definitions().Good(reports[i].commodity).name;
        const char *report = reports[i].condition == "expensive"
                                 ? "These goods fetch high prices here."
                             : reports[i].condition == "cheap" ? "These goods sell cheaply here."
                                                               : "Prices are steady.";
        out += name + " — " + report + "\n\n";
    }
    return out;
}
std::string Escape(std::string input) {
    std::string out;
    for (char c : input) {
        if (c == '<')
            out += "&lt;";
        else if (c == '>')
            out += "&gt;";
        else if (c == '&')
            out += "&amp;";
        else if (c == '\n')
            out += "<br>";
        else
            out += c;
    }
    return out;
}
void Description(RE::TESDescription *self, RE::BSString &out, RE::TESForm *parent,
                 std::uint32_t type) {
    auto match = descriptions.find(self);
    if (active && match != descriptions.end()) {
        std::lock_guard lock(mutex);
        auto [region, paper] = match->second;
        auto text = "<font face=\"$HandwrittenFont\">" + Escape(Text(region, paper)) + "</font>";
        out = text.c_str();
        return;
    }
    descriptionOriginal(self, out, parent, type);
}
void OpenBook(const RE::BSString &description, const RE::ExtraDataList *extra,
              RE::TESObjectREFR *reference, RE::TESObjectBOOK *book, const RE::NiPoint3 &position,
              const RE::NiMatrix3 &rotation, float scale, bool defaultPosition) {
    auto paper = active && installed ? Identify(book) : std::nullopt;
    if (!paper || paper->second != Paper::receipt) {
        openBookOriginal(description, extra, reference, book, position, rotation, scale,
                         defaultPosition);
        return;
    }
    // Bind the physical receipt to its order number, including copies dropped
    // or stored elsewhere. A later order in the same hold cannot rewrite it.
    std::string text = "This order is closed.";
    auto identity = ResolveReceipt(paper->first, extra, reference, book);
    {
        std::lock_guard lock(mutex);
        if (auto *c = economy->FindContract(identity.id);
            c && c->market == boards[paper->first].market && c->status == ContractStatus::accepted)
            text = Terms(*c);
    }
    if (debugLogging)
        SKSE::log::info("Read accepted-order paper: {}, order {}, source {}",
                        boards[paper->first].market, identity.id, identity.source);
    RE::BSString rendered(("<font face=\"$HandwrittenFont\">" + Escape(text) + "</font>").c_str());
    openBookOriginal(rendered, extra, reference, book, position, rotation, scale, defaultPosition);
}
struct InternalScope {
    bool previous = internal;
    InternalScope() {
        internal = true;
    }
    ~InternalScope() {
        internal = previous;
    }
};
void SetCount(RE::TESObjectREFR *owner, RE::TESObjectBOOK *book, int count, int wanted) {
    if (!book || count == wanted)
        return;
    if (count < wanted)
        owner->AddObjectToContainer(book, nullptr, wanted - count, nullptr);
    else
        owner->RemoveItem(book, count - wanted, RE::ITEM_REMOVE_REASON::kRemove, nullptr, nullptr);
}
void ReconcilePapers(bool issueReceipts = false) {
    if (!installed || !active || !economy)
        return;
    receiptGrantPending = receiptGrantPending || issueReceipts;
    auto *ui = RE::UI::GetSingleton();
    auto *player = RE::PlayerCharacter::GetSingleton();
    // Never retire an item while BookMenu still holds its inventory extra data.
    if (!ui || !player || ui->IsMenuOpen(RE::BookMenu::MENU_NAME)) {
        papersDirty = true;
        return;
    }
    InternalScope guard;
    auto carried = player->GetInventory([](auto &form) { return Identify(&form).has_value(); });
    for (unsigned i = 0; i < boards.size(); ++i) {
        auto plan = NoticesFor(Current(i));
        auto *bulletin = Note(i, Paper::bulletin);
        auto *posting = Note(i, Paper::posting);
        auto *receipt = Note(i, Paper::receipt);
        SetCount(player, bulletin, carried[bulletin].first, 0);
        SetCount(player, posting, carried[posting].first, 0);
        auto &[count, entry] = carried[receipt];
        int wanted = plan.ReceiptsToKeep(count, receiptGrantPending);
        auto *contract = Current(i);
        bool matches = false;
        if (wanted && count == 1 && entry && entry->extraLists)
            for (auto *extra : *entry->extraLists)
                matches = matches || (extra && contract && ReceiptID(i, extra) == contract->id);
        if (!matches) {
            SetCount(player, receipt, count, 0);
            if (wanted) {
                // Skyrim owns the extra list after AddObjectToContainer and
                // serializes its custom name with the ordinary inventory item.
                auto *extra = new RE::ExtraDataList();
                extra->Add(new RE::ExtraTextDisplayData(ReceiptName(i, contract->id).c_str()));
                player->AddObjectToContainer(receipt, extra, 1, nullptr);
            }
        }
        if (debugLogging && (carried[bulletin].first || carried[posting].first || count != wanted ||
                             (wanted && !matches)))
            SKSE::log::info("Papers {}: retired {} bulletins and {} postings; receipts {} -> {}",
                            boards[i].market, carried[bulletin].first, carried[posting].first,
                            count, wanted);
        if (auto *board = BoardRef(i)) {
            auto posted = board->GetInventoryCounts([=](auto &form) {
                return &form == bulletin || &form == posting || &form == receipt;
            });
            SetCount(board, bulletin, posted[bulletin], plan.bulletinOnBoard);
            SetCount(board, posting, posted[posting], plan.orderOnBoard);
            // The old contract form is now the accepted receipt, never a posting.
            SetCount(board, receipt, posted[receipt], 0);
        }
    }
    papersDirty = false;
    receiptGrantPending = false;
    if (auto menu = ui->GetMenu<RE::ContainerMenu>())
        if (auto *list = menu->GetRuntimeData().itemList)
            list->Update();
    if (auto menu = ui->GetMenu<RE::InventoryMenu>())
        if (auto *list = menu->GetRuntimeData().itemList)
            list->Update();
}
void QueuePaperCleanup() {
    std::lock_guard lock(mutex);
    papersDirty = true;
    if (paperCleanupQueued)
        return;
    paperCleanupQueued = true;
    auto gen = generation.load();
    SKSE::GetTaskInterface()->AddTask([gen] {
        std::lock_guard lock(mutex);
        if (generation != gen)
            return;
        paperCleanupQueued = false;
        if (active)
            ReconcilePapers();
    });
}
void ShowBoard(unsigned region, std::uint64_t displayedID);
std::optional<std::pair<unsigned, Paper>> SelectedPosting(RE::ContainerMenu *menu) {
    auto *list = menu ? menu->GetRuntimeData().itemList : nullptr;
    auto *item = SelectedItem(list);
    auto paper = item && item->data.objDesc ? Identify(item->data.objDesc->object) : std::nullopt;
    if (!paper || paper->second == Paper::receipt)
        return std::nullopt;
    auto target = RE::TESObjectREFR::LookupByHandle(RE::ContainerMenu::GetTargetRefHandle());
    if (!target || target.get() != BoardRef(paper->first))
        return std::nullopt;
    return paper;
}
bool ReadSelectedPosting(RE::ContainerMenu *menu) {
    if (!active || !installed)
        return false;
    auto paper = SelectedPosting(menu);
    if (!paper)
        return false;
    std::lock_guard lock(mutex);
    // Prevent double-clicks from scheduling overlapping book/menu sessions.
    if (reading)
        return true;
    auto *contract = Current(paper->first);
    Reading requested{paper->first,      paper->second,  contract ? contract->id : 0,
                      generation.load(), ++readSequence, false};
    reading = requested;
    SKSE::GetTaskInterface()->AddTask([requested] {
        std::lock_guard lock(mutex);
        if (!active || generation != requested.generation || !reading ||
            reading->sequence != requested.sequence)
            return;
        auto *ui = RE::UI::GetSingleton();
        auto *current = Current(requested.region);
        auto target = RE::TESObjectREFR::LookupByHandle(RE::ContainerMenu::GetTargetRefHandle());
        if (!target || target.get() != BoardRef(requested.region) || !Nearby(requested.region) ||
            !ui || !ui->IsMenuOpen(RE::ContainerMenu::MENU_NAME) ||
            ui->IsMenuOpen(RE::BookMenu::MENU_NAME) ||
            (requested.paper == Paper::posting &&
             (!current || current->id != requested.contract))) {
            reading.reset();
            return;
        }
        if (auto *book = Note(requested.region, requested.paper)) {
            // No reference or inventory extra list is passed: BookMenu offers
            // reading only, and neither posting is transferred to the player.
            RE::BookMenu::OpenMenuFromBaseForm(book);
            if (debugLogging)
                SKSE::log::info("Read board posting: {}, contract {}",
                                boards[requested.region].market, requested.contract);
        } else
            reading.reset();
    });
    return true;
}
void Transfer(const RE::FxDelegateArgs &args) {
    if (args.GetArgCount() >= 2 && args[1].IsBool() && args[1].GetBool() &&
        ReadSelectedPosting(static_cast<RE::ContainerMenu *>(args.GetHandler())))
        return;
    if (transferOriginal)
        transferOriginal(args);
}
void Equip(const RE::FxDelegateArgs &args) {
    if (ReadSelectedPosting(static_cast<RE::ContainerMenu *>(args.GetHandler())))
        return;
    if (equipOriginal)
        equipOriginal(args);
}
void DisabledSelect(const RE::FxDelegateArgs &args) {
    if (!ReadSelectedPosting(static_cast<RE::ContainerMenu *>(args.GetHandler())) &&
        disabledOriginal)
        disabledOriginal(args);
}
void TakeAll(const RE::FxDelegateArgs &args) {
    // Preserve native Take All for ordinary Missives and other addons. Some
    // menu replacements bypass BOOK's Can't Take flag, so the event sink and
    // this deferred cleanup also return any transferred postings to the board.
    bool board = active && installed && AtBoardContainer();
    if (takeAllOriginal)
        takeAllOriginal(args);
    if (board)
        QueuePaperCleanup();
}
class ContainerProcessor final : public RE::FxDelegateHandler::CallbackProcessor {
    RE::FxDelegateHandler::CallbackProcessor *downstream_;

  public:
    explicit ContainerProcessor(RE::FxDelegateHandler::CallbackProcessor *p) : downstream_(p) {}
    void Process(const RE::GString &name, RE::FxDelegateHandler::CallbackFn *method) override {
        auto pass = [&](const char *key, auto &original, auto replacement) {
            if (std::strcmp(name.c_str(), key) != 0)
                return false;
            original = method;
            downstream_->Process(name, replacement);
            return true;
        };
        if (!pass("ItemTransfer", transferOriginal, Transfer) &&
            !pass("EquipItem", equipOriginal, Equip) &&
            !pass("TakeAllItems", takeAllOriginal, TakeAll) &&
            !pass("DisabledItemSelect", disabledOriginal, DisabledSelect))
            downstream_->Process(name, method);
    }
};
void ContainerAccept(RE::ContainerMenu *menu, RE::FxDelegateHandler::CallbackProcessor *p) {
    ContainerProcessor proxy(p);
    containerAcceptOriginal(menu, &proxy);
}
class ReadPostingAction final : public RE::GFxFunctionHandler {
    RE::GFxValue original_;

  public:
    explicit ReadPostingAction(RE::GFxValue original) : original_(std::move(original)) {}
    void Call(Params &p) override {
        if (!p.thisPtr || !p.thisPtr->IsObject() || (p.argCount && !p.args))
            return;
        auto *ui = RE::UI::GetSingleton();
        auto menu = ui ? ui->GetMenu<RE::ContainerMenu>() : nullptr;
        // Can't Take can disable a row before SkyUI dispatches any native
        // callback. Read our postings before that check, including equip mode.
        if (ReadSelectedPosting(menu.get())) {
            p.thisPtr->SetMember("_equipHand", RE::GFxValue());
            return;
        }
        std::vector<RE::GFxValue> args{*p.thisPtr};
        for (unsigned i = 0; i < p.argCount; ++i)
            args.push_back(p.args[i]);
        RE::GFxValue ignored;
        original_.Invoke("call", p.retVal ? p.retVal : &ignored, args.data(),
                         static_cast<std::uint32_t>(args.size()));
    }
};
class ReadButton final : public RE::GFxFunctionHandler {
    RE::GFxValue original_;

  public:
    explicit ReadButton(RE::GFxValue original) : original_(std::move(original)) {}
    void Call(Params &p) override {
        if (!p.thisPtr || !p.thisPtr->IsObject() || (p.argCount && !p.args))
            return;
        std::vector<RE::GFxValue> args{*p.thisPtr};
        for (unsigned i = 0; i < p.argCount; ++i)
            args.push_back(p.args[i]);
        RE::GFxValue ignored;
        original_.Invoke("call", p.retVal ? p.retVal : &ignored, args.data(),
                         static_cast<std::uint32_t>(args.size()));
        auto *ui = RE::UI::GetSingleton();
        auto menu = ui ? ui->GetMenu<RE::ContainerMenu>() : nullptr;
        if (!active || !installed || !p.argCount || !p.args[0].IsBool() || !p.args[0].GetBool() ||
            !SelectedPosting(menu.get()))
            return;
        ShowPostingReadButton(p.thisPtr);
    }
};
void ContainerAdvance(RE::ContainerMenu *menu, float interval, std::uint32_t time) {
    containerAdvanceOriginal(menu, interval, time);
    if (!active || !installed || !menu || !menu->uiMovie)
        return;
    auto &root = menu->GetRuntimeData().root;
    RE::GFxValue marker, config, original, transfer;
    if (!root.IsObject() || root.GetMember("MarcoPoloReadButton", &marker) || !AtBoardContainer() ||
        !root.GetMember("_config", &config) || !config.IsObject() ||
        !root.GetMember("updateBottomBar", &original) || !original.IsObject() ||
        !root.GetMember("startItemTransfer", &transfer) || !transfer.IsObject())
        return;
    auto handler = RE::make_gptr<ReadButton>(original);
    RE::GFxValue replacement;
    menu->uiMovie->CreateFunction(&replacement, handler.get());
    root.SetMember("updateBottomBar", replacement);
    auto action = RE::make_gptr<ReadPostingAction>(transfer);
    menu->uiMovie->CreateFunction(&replacement, action.get());
    root.SetMember("startItemTransfer", replacement);
    root.SetMember("MarcoPoloReadButton", RE::GFxValue(true));
    if (SelectedPosting(menu)) {
        RE::GFxValue selected(true);
        root.Invoke("updateBottomBar", nullptr, &selected, 1);
    }
}
class Response final : public RE::IMessageBoxCallback {
    std::uint64_t id_, gen_;
    bool accepted_;

  public:
    Response(std::uint64_t id, bool accepted)
        : id_(id), gen_(generation.load()), accepted_(accepted) {}
    void Run(std::uint8_t message) override {
        int button = static_cast<int>(message);
        auto id = id_, gen = gen_;
        int action = button == 0 ? (accepted_ ? 1 : 0) : button == 1 && accepted_ ? 2 : -1;
        if (action >= 0)
            SKSE::GetTaskInterface()->AddTask([id, gen, action] {
                if (active && generation == gen)
                    ContractAction(id, action);
            });
    }
};
class ReadEvents final : public RE::BSTEventSink<RE::MenuOpenCloseEvent>,
                         public RE::BSTEventSink<RE::TESContainerChangedEvent> {
  public:
    RE::BSEventNotifyControl ProcessEvent(const RE::MenuOpenCloseEvent *e,
                                          RE::BSTEventSource<RE::MenuOpenCloseEvent> *) override {
        if (!e || !active || !installed ||
            (e->menuName != RE::BookMenu::MENU_NAME &&
             e->menuName != RE::ContainerMenu::MENU_NAME &&
             e->menuName != RE::InventoryMenu::MENU_NAME))
            return RE::BSEventNotifyControl::kContinue;
        std::lock_guard lock(mutex);
        if (e->menuName == RE::BookMenu::MENU_NAME) {
            if (e->opening) {
                if (reading && reading->generation == generation &&
                    RE::BookMenu::GetTargetForm() == Note(reading->region, reading->paper))
                    reading->opened = true;
                else
                    reading.reset();
            } else {
                auto finished = reading;
                reading.reset();
                if (finished && finished->opened && finished->paper == Paper::posting)
                    SKSE::GetTaskInterface()->AddTask([finished = *finished] {
                        std::lock_guard lock(mutex);
                        if (active && generation == finished.generation)
                            ShowBoard(finished.region, finished.contract);
                    });
                if (papersDirty)
                    QueuePaperCleanup();
            }
        } else if (e->opening && papersDirty) {
            QueuePaperCleanup();
        } else if (!e->opening && e->menuName == RE::ContainerMenu::MENU_NAME && reading &&
                   !reading->opened) {
            reading.reset();
        }
        return RE::BSEventNotifyControl::kContinue;
    }
    RE::BSEventNotifyControl
    ProcessEvent(const RE::TESContainerChangedEvent *e,
                 RE::BSTEventSource<RE::TESContainerChangedEvent> *) override {
        if (e && active && installed && !internal && e->itemCount > 0 &&
            (e->oldContainer == 0x14 || e->newContainer == 0x14) &&
            Identify(RE::TESForm::LookupByID(e->baseObj)))
            QueuePaperCleanup();
        return RE::BSEventNotifyControl::kContinue;
    }
};
ReadEvents readEvents;
void ShowBoard(unsigned region, std::uint64_t displayedID) {
    if (!installed || !active || region >= boards.size() || !Nearby(region))
        return;
    auto *c = Current(region);
    if (!c || c->id != displayedID)
        return;
    bool accepted = c->status == ContractStatus::accepted;
    if (accepted)
        ReconcilePapers(true); // The issuing board can replace a lost receipt.
    RE::BSString text(Terms(*c).c_str());
    RE::BSTArray<RE::BSString> buttons;
    buttons.push_back(accepted ? "Deliver" : "Accept");
    if (accepted)
        buttons.push_back("Cancel contract");
    buttons.push_back("Close");
    RE::BSTSmartPointer<RE::IMessageBoxCallback> callback(new Response(c->id, accepted));
    RE::MessageBoxMenu::Create(text, callback, 0, 0, 10, buttons);
}
bool IsPosting(RE::InventoryEntryData *entry) {
    auto paper = entry ? Identify(entry->object) : std::nullopt;
    return paper && paper->second != Paper::receipt;
}
void FilterQuickLoot(QuickLoot::API::ModifyInventoryEvent *event) {
    if (!event || !installed || !active)
        return;
    for (auto it = event->inventory.begin(); it != event->inventory.end();) {
        if (IsPosting(it->entry)) {
            // The API transfers ownership of these copied inventory entries.
            // Deleting one frees its list nodes, not the engine's extra data.
            delete it->entry;
            it = event->inventory.erase(it);
        } else
            ++it;
    }
}
void StopQuickLootPosting(QuickLoot::API::TakingItemEvent *event) {
    if (event && installed && active && event->stack && IsPosting(event->stack->entry))
        event->result = QuickLoot::API::HandleResult::kStop;
}
void RegisterQuickLoot() {
    using namespace QuickLoot::API;
    // API 2.0 is sufficient; no DLL dependency or Skyrim-version condition.
    if (!QuickLootAPI::Init("MarcoPolo", ApiVersion::kV20)) {
        SKSE::log::info("QuickLoot IE API unavailable; Missives deferred pickup cleanup retained");
        return;
    }
    QuickLootAPI::RegisterModifyInventoryHandler(FilterQuickLoot);
    QuickLootAPI::RegisterTakingItemHandler(StopQuickLootPosting);
    SKSE::log::info("QuickLoot IE: Marco Polo board postings excluded from loot and Take All");
}
} // namespace
bool InstallMissives() {
    for (unsigned i = 0; i < boards.size(); ++i)
        if (!Note(i, Paper::bulletin) || !Note(i, Paper::posting) || !Note(i, Paper::receipt) ||
            !BoardRef(i)) {
            SKSE::log::info(
                "Missives addon unavailable; requires the 27-record Missives ESP (0.1.6+)");
            return false;
        }
    for (unsigned i = 0; i < boards.size(); ++i)
        for (auto kind : {Paper::bulletin, Paper::posting, Paper::receipt})
            descriptions.emplace(static_cast<RE::TESDescription *>(Note(i, kind)),
                                 std::pair{i, kind});
    auto init = MH_Initialize();
    if (init != MH_OK && init != MH_ERROR_ALREADY_INITIALIZED)
        return false;
    REL::Relocation<std::uintptr_t> target{RELOCATION_ID(14399, 14552)};
    REL::Relocation<std::uintptr_t> openTarget{RELOCATION_ID(50122, 51053)};
    if (MH_CreateHook(reinterpret_cast<void *>(target.address()),
                      reinterpret_cast<void *>(Description),
                      reinterpret_cast<void **>(&descriptionOriginal)) != MH_OK) {
        SKSE::log::error("Missives text hook failed; addon disabled");
        return false;
    }
    if (MH_CreateHook(reinterpret_cast<void *>(openTarget.address()),
                      reinterpret_cast<void *>(OpenBook),
                      reinterpret_cast<void **>(&openBookOriginal)) != MH_OK ||
        MH_EnableHook(reinterpret_cast<void *>(target.address())) != MH_OK ||
        MH_EnableHook(reinterpret_cast<void *>(openTarget.address())) != MH_OK) {
        MH_DisableHook(reinterpret_cast<void *>(target.address()));
        MH_RemoveHook(reinterpret_cast<void *>(target.address()));
        MH_RemoveHook(reinterpret_cast<void *>(openTarget.address()));
        SKSE::log::error("Missives book hook failed; addon disabled");
        return false;
    }
    REL::Relocation<std::uintptr_t> table{RE::VTABLE_ContainerMenu[0]};
    containerAcceptOriginal = table.write_vfunc(hooks::ContainerAcceptSlot, ContainerAccept);
    containerAdvanceOriginal = table.write_vfunc(hooks::ContainerAdvanceSlot, ContainerAdvance);
    RE::UI::GetSingleton()->AddEventSink<RE::MenuOpenCloseEvent>(&readEvents);
    RE::ScriptEventSourceHolder::GetSingleton()->AddEventSink<RE::TESContainerChangedEvent>(
        &readEvents);
    installed = true;
    RegisterQuickLoot();
    SKSE::log::info("Missives notices and supply contracts enabled");
    return true;
}
void ResetMissives() {
    reading.reset();
    paperCleanupQueued = false;
    papersDirty = true;
    receiptGrantPending = false;
}
void UpdateBoards(bool issueReceipts) {
    if (!installed || !active)
        return;
    for (unsigned i = 0; i < boards.size(); ++i) {
        auto *board = BoardRef(i);
        if (!board)
            continue;
        bool issuedToday = std::any_of(
            economy->Get().contracts.begin(), economy->Get().contracts.end(), [&](auto &c) {
                return c.market == boards[i].market && c.issued >= economy->Get().day;
            });
        if (!Current(i) && !issuedToday) {
            auto reports = economy->Reports(boards[i].market);
            std::stable_sort(reports.begin(), reports.end(),
                             [](auto &a, auto &b) { return a.multiplier > b.multiplier; });
            for (auto &report : reports) {
                auto found = std::find_if(delivery.begin(), delivery.end(), [&](auto &pair) {
                    return pair.first == report.commodity;
                });
                if (found == delivery.end())
                    continue;
                auto *form = RE::TESForm::LookupByID<RE::TESBoundObject>(found->second);
                if (!form || Classify(form) != report.commodity)
                    continue;
                RE::InventoryEntryData entry(form, 1);
                if (economy->Offer(boards[i].market, report.commodity, 30, Key(form),
                                   form->GetName(), RawValue(&entry)))
                    break;
            }
        }
    }
    ReconcilePapers(issueReceipts);
}
void ContractAction(std::uint64_t id, int action) {
    std::lock_guard lock(mutex);
    if (!installed || !active || !economy)
        return;
    double now = RE::Calendar::GetSingleton()->GetDaysPassed();
    economy->Advance(now, 32);
    if (economy->Get().day + 1 <= now) {
        RE::SendHUDMessage::ShowHUDMessage(
            "Marco Polo is catching up. Try the contract again shortly.");
        return;
    }
    auto *found = economy->FindContract(id);
    if (!found)
        return;
    Contract c = *found;
    unsigned region = 0;
    while (region < boards.size() && c.market != boards[region].market)
        ++region;
    if (region == boards.size() ||
        !BoardActionAllowed(Current(region), id, action, Nearby(region))) {
        // Expiry can happen while a notice or confirmation is open in an
        // unpaused menu. Retire its receipt without acting on a newer order.
        ReconcilePapers();
        return;
    }
    if (action == 0) {
        if (economy->Accept(id))
            RE::SendHUDMessage::ShowHUDMessage("Marco Polo: supply contract accepted.");
    } else if (action == 2) {
        if (economy->Cancel(id))
            RE::SendHUDMessage::ShowHUDMessage("Marco Polo: supply contract cancelled.");
    } else if (action == 1) {
        if (c.status != ContractStatus::accepted || economy->Get().day > c.deadline)
            return;
        auto *f = Resolve(c.item);
        auto *form = f ? f->As<RE::TESBoundObject>() : nullptr;
        auto *p = RE::PlayerCharacter::GetSingleton();
        if (!form || !p)
            return;
        auto inventory = p->GetInventory([form](auto &obj) { return &obj == form; });
        auto it = inventory.find(form);
        if (it == inventory.end() || it->second.first < c.quantity ||
            !PlainItem(it->second.second.get())) {
            RE::SendHUDMessage::ShowHUDMessage(
                "You do not have the complete order of ordinary goods.");
            return;
        }
        int before = it->second.first;
        p->RemoveItem(form, c.quantity, RE::ITEM_REMOVE_REASON::kRemove, nullptr, nullptr);
        int after = p->GetInventoryCounts([form](auto &obj) { return &obj == form; })[form];
        int removed = before - after;
        if (removed != c.quantity) {
            if (removed > 0)
                p->AddObjectToContainer(form, nullptr, removed, nullptr);
            RE::SendHUDMessage::ShowHUDMessage(
                "Delivery could not be completed. Your goods were returned.");
            return;
        }
        Money reward = economy->Deliver(id, c.quantity);
        if (!reward) {
            p->AddObjectToContainer(form, nullptr, c.quantity, nullptr);
            return;
        }
        p->AddObjectToContainer(RE::TESForm::LookupByID<RE::TESBoundObject>(0xF), nullptr,
                                static_cast<int>(reward), nullptr);
        RE::SendHUDMessage::ShowHUDMessage(
            std::format("Marco Polo: order delivered. {} gold paid.", reward).c_str());
    }
    UpdateBoards(action == 0);
    Publish();
}
} // namespace marco::runtime
