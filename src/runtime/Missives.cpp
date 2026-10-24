#include "Runtime.h"
#include <MinHook.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <format>

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
int pendingBoard = -1;
std::uint64_t pendingGeneration{};
using DescriptionFn = void (*)(RE::TESDescription *, RE::BSString &, RE::TESForm *, std::uint32_t);
DescriptionFn descriptionOriginal{};
RE::TESObjectBOOK *Note(unsigned region, bool contract) {
    return RE::TESDataHandler::GetSingleton()->LookupForm<RE::TESObjectBOOK>(
        0x800 + region * 2 + (contract ? 1 : 0), Addon);
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
std::string Terms(const Contract &c) {
    auto hold = economy->Definitions().Market(c.market).name;
    return std::format("Goods wanted\n\n{}: {}\nPayment: {} septims\nDeliver by {}\n"
                       "Delivery: {} noticeboard\n\n— The merchants of {}",
                       c.itemName, c.quantity, c.reward, DateForDay(c.deadline), hold, hold);
}
std::string Text(unsigned region, bool contract) {
    if (contract) {
        if (auto *c = Current(region))
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
        const char *report =
            reports[i].condition == "shortage"  ? "Stocks are running low. Prices are high."
            : reports[i].condition == "surplus" ? "Plenty for sale, at low prices."
                                                : "Supplies and prices are steady.";
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
    if (active) {
        std::lock_guard lock(mutex);
        for (unsigned i = 0; i < boards.size(); ++i)
            for (bool contract : {false, true})
                if (auto *book = Note(i, contract);
                    book && static_cast<RE::TESDescription *>(book) == self) {
                    auto text =
                        "<font face=\"$HandwrittenFont\">" + Escape(Text(i, contract)) + "</font>";
                    out = text.c_str();
                    return;
                }
    }
    descriptionOriginal(self, out, parent, type);
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
class ReadEvents final : public RE::BSTEventSink<RE::MenuOpenCloseEvent> {
  public:
    RE::BSEventNotifyControl ProcessEvent(const RE::MenuOpenCloseEvent *e,
                                          RE::BSTEventSource<RE::MenuOpenCloseEvent> *) override {
        if (!e || e->menuName != RE::BookMenu::MENU_NAME)
            return RE::BSEventNotifyControl::kContinue;
        if (e->opening) {
            pendingBoard = -1;
            if (active) {
                auto *book = RE::BookMenu::GetTargetForm();
                for (unsigned i = 0; i < boards.size(); ++i)
                    if (book && book == Note(i, true)) {
                        pendingBoard = static_cast<int>(i);
                        pendingGeneration = generation.load();
                        break;
                    }
            }
        } else if (pendingBoard >= 0) {
            int region = pendingBoard;
            auto gen = pendingGeneration;
            pendingBoard = -1;
            SKSE::GetTaskInterface()->AddTask([region, gen] {
                if (active && generation == gen)
                    ShowBoard(region);
            });
        }
        return RE::BSEventNotifyControl::kContinue;
    }
};
ReadEvents readEvents;
} // namespace
bool InstallMissives() {
    if (!Note(0, false) || !BoardRef(0))
        return false;
    auto init = MH_Initialize();
    if (init != MH_OK && init != MH_ERROR_ALREADY_INITIALIZED)
        return false;
    REL::Relocation<std::uintptr_t> target{RELOCATION_ID(14399, 14552)};
    if (MH_CreateHook(reinterpret_cast<void *>(target.address()),
                      reinterpret_cast<void *>(Description),
                      reinterpret_cast<void **>(&descriptionOriginal)) != MH_OK ||
        MH_EnableHook(reinterpret_cast<void *>(target.address())) != MH_OK) {
        SKSE::log::error("Missives text hook failed; addon disabled");
        return false;
    }
    RE::UI::GetSingleton()->AddEventSink<RE::MenuOpenCloseEvent>(&readEvents);
    installed = true;
    SKSE::log::info("Missives notices and supply contracts enabled");
    return true;
}
void UpdateBoards() {
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
        for (bool job : {false, true})
            if (auto *book = Note(i, job)) {
                auto items = board->GetInventoryCounts([book](auto &f) { return &f == book; });
                int count = items[book];
                bool wanted = !job || Current(i);
                if (wanted && count == 0)
                    board->AddObjectToContainer(book, nullptr, 1, nullptr);
                if (count > (wanted ? 1 : 0))
                    board->RemoveItem(book, count - (wanted ? 1 : 0),
                                      RE::ITEM_REMOVE_REASON::kRemove, nullptr, nullptr);
            }
    }
}
void ShowBoard(int region) {
    std::lock_guard lock(mutex);
    if (!installed || !active || region < 0 || region >= static_cast<int>(boards.size()))
        return;
    auto *c = Current(static_cast<unsigned>(region));
    if (!c)
        return;
    bool accepted = c->status == ContractStatus::accepted;
    RE::BSString text(Terms(*c).c_str());
    RE::BSTArray<RE::BSString> buttons;
    buttons.push_back(accepted ? "Deliver" : "Accept");
    if (accepted)
        buttons.push_back("Cancel contract");
    buttons.push_back("Close");
    RE::BSTSmartPointer<RE::IMessageBoxCallback> callback(new Response(c->id, accepted));
    RE::MessageBoxMenu::Create(text, callback, 0, 0, 10, buttons);
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
    if (action == 0) {
        if (economy->Accept(id))
            RE::SendHUDMessage::ShowHUDMessage("Marco Polo: supply contract accepted.");
    } else if (action == 2) {
        if (economy->Cancel(id))
            RE::SendHUDMessage::ShowHUDMessage("Marco Polo: supply contract cancelled.");
    } else if (action == 1) {
        unsigned region = 0;
        while (region < boards.size() && c.market != boards[region].market)
            ++region;
        if (region == boards.size() || !Nearby(region)) {
            RE::SendHUDMessage::ShowHUDMessage("Deliver at the issuing hold's Missives board.");
            return;
        }
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
    UpdateBoards();
    Publish();
}
} // namespace marco::runtime
