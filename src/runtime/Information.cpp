#include "Runtime.h"
#include <algorithm>
#include <cmath>
#include <format>

namespace marco::runtime {
namespace {
RE::FormID lastSettlement{};
double lastObservationDay{-1};
std::string lastRegion;
constexpr char RumorPlugin[] = "MarcoPolo - Trade Rumors.esp";
double Now() {
    auto *calendar = RE::Calendar::GetSingleton();
    return calendar ? calendar->GetDaysPassed() : 0;
}
RE::BGSLocation *Settlement(RE::TESObjectREFR *ref) {
    auto *location = ref ? ref->GetCurrentLocation() : nullptr;
    for (unsigned depth = 0; location && depth < 64; ++depth, location = location->parentLoc)
        if (location->HasKeywordString("LocTypeCity") ||
            location->HasKeywordString("LocTypeTown") || location->HasKeywordString("LocTypeInn"))
            return location;
    return nullptr;
}
bool Innkeeper(RE::Actor *actor) {
    if (!actor || actor->IsDead())
        return false;
    auto *job = RE::TESForm::LookupByID<RE::TESFaction>(0x5091B);
    auto *rooms = RE::TESForm::LookupByID<RE::TESFaction>(0x9CC97);
    return (job && actor->IsInFaction(job)) || (rooms && actor->IsInFaction(rooms));
}
void LogRumorDialogue() {
    auto *ui = RE::UI::GetSingleton();
    if (!ui || !ui->IsMenuOpen(RE::DialogueMenu::MENU_NAME))
        return;
    auto *manager = RE::MenuTopicManager::GetSingleton();
    auto ref = manager ? manager->speaker.get() : RE::NiPointer<RE::TESObjectREFR>{};
    auto *actor = ref ? ref->As<RE::Actor>() : nullptr;
    auto *player = RE::PlayerCharacter::GetSingleton();
    auto *data = RE::TESDataHandler::GetSingleton();
    if (!actor || !player || !data) {
        SKSE::log::info("Trade-rumor dialogue: speaker or game data not yet available");
        return;
    }
    auto *quest = data->LookupForm<RE::TESQuest>(0x800, RumorPlugin);
    auto *branch = data->LookupForm<RE::BGSDialogueBranch>(0x801, RumorPlugin);
    auto *topic = data->LookupForm<RE::TESTopic>(0x802, RumorPlugin);
    auto *global = data->LookupForm<RE::TESGlobal>(0x804, RumorPlugin);
    bool linked = quest && branch && topic && branch->quest == quest &&
                  branch->startingTopic == topic && topic->ownerQuest == quest &&
                  topic->ownerBranch == branch;
    auto count = topic ? topic->numTopicInfos : 0;
    unsigned matching{}, linkedInfos{};
    RE::FormID selected{}, response{};
    if (topic && topic->topicInfos && count <= 64)
        for (unsigned i = 0; i < count; ++i) {
            auto *info = topic->topicInfos[i];
            if (!info)
                continue;
            linkedInfos += info->parentTopic == topic;
            if (info->objConditions.IsTrue(actor, player)) {
                ++matching;
                if (!selected) {
                    selected = info->GetFormID();
                    response = info->dataInfo ? info->dataInfo->GetFormID() : 0;
                }
            }
        }
    SKSE::log::info("Trade-rumor dialogue: {} ({:08X}), innkeeper {}, active {}, global {}, "
                    "quest running {}, quest enabled {}, links {}, linked INFOs {}/{}, "
                    "matching INFOs {}, first match {:08X}, shared response {:08X}",
                    actor->GetName(), actor->GetFormID(), Innkeeper(actor), active.load(),
                    global ? global->value : -1.0f, quest && quest->IsRunning(),
                    quest && quest->IsEnabled(), linked, linkedInfos, count, matching, selected,
                    response);
}
class DialogueEvents final : public RE::BSTEventSink<RE::MenuOpenCloseEvent> {
    RE::BSEventNotifyControl ProcessEvent(const RE::MenuOpenCloseEvent *event,
                                          RE::BSTEventSource<RE::MenuOpenCloseEvent> *) override {
        if (debugLogging && event && event->opening &&
            event->menuName == RE::DialogueMenu::MENU_NAME) {
            auto gen = generation.load();
            // Run after menu-open dispatch, when the speaker handle is set.
            SKSE::GetTaskInterface()->AddTask([gen] {
                std::lock_guard lock(mutex);
                if (debugLogging && generation == gen)
                    LogRumorDialogue();
            });
        }
        return RE::BSEventNotifyControl::kContinue;
    }
} dialogueEvents;
void HearTradeRumors(RE::StaticFunctionTag *, RE::Actor *speaker) {
    // Papyrus may call on a VM worker. Only retain identity; all game access and
    // popup creation below run through SKSE's game-thread task queue.
    if (!speaker)
        return;
    auto id = speaker->GetFormID();
    auto gen = generation.load();
    SKSE::GetTaskInterface()->AddTask([id, gen] {
        try {
            std::string text;
            {
                std::lock_guard lock(mutex);
                if (!active || generation != gen || !economy)
                    return;
                auto *actor = RE::TESForm::LookupByID<RE::Actor>(id);
                auto *player = RE::PlayerCharacter::GetSingleton();
                if (!Innkeeper(actor) || !player ||
                    actor->GetParentCell() != player->GetParentCell() ||
                    actor->GetPosition().GetDistance(player->GetPosition()) > 1000)
                    return;
                auto market = Region(actor);
                if (market.empty())
                    market = Region(player);
                if (market.empty()) {
                    text = "I haven't heard any trade news from these parts.";
                } else {
                    double now = Now();
                    AdvanceEconomy(now, 32);
                    if (economy->Get().day + 1 <= now) {
                        text = "Give me a moment to gather my thoughts.";
                    } else {
                        std::string source = actor->GetName();
                        if (source.empty())
                            source = "Innkeeper";
                        source = source.substr(0, 180);
                        auto batch = knowledge.Hear(*economy, market, now, source);
                        text = source + "\n\n";
                        for (auto &r : batch.reports) {
                            text += Knowledge::RumorText(r, economy->Definitions());
                            auto age = static_cast<int>(
                                std::max(0.0, std::floor(now) - std::floor(r.observed)));
                            text += age == 0 ? " The news is from today.\n\n"
                                             : std::format(" That news is {} day{} old.\n\n", age,
                                                           age == 1 ? "" : "s");
                        }
                        if (batch.reports.empty())
                            text += "Nothing new has reached me.";
                        if (debugLogging)
                            SKSE::log::info("Trade rumors: {} in {}, {} reports, batch day {}",
                                            source, market, batch.reports.size(), batch.heard);
                    }
                }
            }
            // Release our lock before entering another engine/UI callback.
            if (!text.empty())
                RE::DebugMessageBox(text.c_str());
        } catch (const std::exception &ex) {
            SKSE::log::error("Trade rumors: {}", ex.what());
        }
    });
}
} // namespace
void ResetInformation() {
    lastSettlement = 0;
    lastObservationDay = -1;
    lastRegion.clear();
}
void RegisterInformation() {
    static bool registered{};
    auto *ui = RE::UI::GetSingleton();
    if (debugLogging && !registered && ui) {
        ui->AddEventSink<RE::MenuOpenCloseEvent>(&dialogueEvents);
        registered = true;
    }
}
void SetRumorsEnabled(bool value) {
    auto *data = RE::TESDataHandler::GetSingleton();
    auto *global = data ? data->LookupForm<RE::TESGlobal>(0x804, RumorPlugin) : nullptr;
    if (global)
        global->value = value ? 1.0f : 0.0f;
    if (debugLogging)
        SKSE::log::info("Trade-rumor addon: availability record {}, enabled {}",
                        global ? global->GetFormID() : 0, global && value);
}
void ObserveMarket(const std::string &market, const std::string &source) {
    double now = Now();
    if (!economy || market.empty() || !std::isfinite(now) || now < economy->Get().day ||
        economy->Get().day + 1 <= now)
        return;
    knowledge.Observe(*economy, market, now,
                      source.empty() ? "Local market" : source.substr(0, 180));
}
bool ObserveSettlement() {
    auto *player = RE::PlayerCharacter::GetSingleton();
    auto market = Region(player);
    bool moved = market != lastRegion;
    lastRegion = market;
    auto *settlement = Settlement(player);
    auto id = settlement ? settlement->GetFormID() : 0;
    double day = std::floor(Now());
    bool changed = id && !market.empty() && (id != lastSettlement || day != lastObservationDay);
    lastSettlement = id;
    if (changed) {
        ObserveMarket(market, settlement->GetName());
        lastObservationDay = day;
        if (debugLogging)
            SKSE::log::info("Observed market {} at {}", market, settlement->GetName());
    }
    return changed || moved;
}
bool RegisterPapyrus(RE::BSScript::IVirtualMachine *vm) {
    vm->RegisterFunction("HearTradeRumors", "MarcoPolo_Native", HearTradeRumors);
    return true;
}
} // namespace marco::runtime
