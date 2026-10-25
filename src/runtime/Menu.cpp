#include "Runtime.h"
#include "SKSEMenuFramework.h"
#include <algorithm>
#include <cmath>
#include <format>

namespace marco::runtime {
namespace {
namespace UI = ImGuiMCP;
int selected = -1, debugSelected = -1;
std::string Age(double day, double now) {
    int age = static_cast<int>(std::max(0.0, std::floor(now) - std::floor(day)));
    return age == 0 ? "today"
                    : std::format("{} day{} ago{}", age, age == 1 ? "" : "s",
                                  age >= 7 ? " (stale)" : "");
}
std::string Rate(const TradeReport &r) {
    return r.rumor ? std::format("{:.0f}-{:.0f}%", r.low * 100, r.high * 100)
                   : std::format("{:.0f}%", r.low * 100);
}
bool Header(const Snapshot &view, const char *title) {
    UI::TextUnformatted(title);
    if (!view.ready) {
        UI::TextWrapped("%s", view.status.c_str());
        UI::TextUnformatted("Load a game to see your trade reports.");
        return false;
    }
    UI::TextUnformatted(view.date.c_str());
    return true;
}
std::string ChooseMarket(const Snapshot &view, int &selection) {
    std::vector<std::string> ids;
    std::vector<const char *> names;
    for (auto &[id, name] : view.marketNames) {
        ids.push_back(id);
        names.push_back(name.c_str());
    }
    if (ids.empty())
        return {};
    if (selection < 0) {
        auto current = std::find(ids.begin(), ids.end(), view.market);
        selection = current == ids.end() ? 0 : static_cast<int>(current - ids.begin());
    }
    selection = std::clamp(selection, 0, static_cast<int>(ids.size()) - 1);
    UI::Combo("Market", &selection, names.data(), static_cast<int>(names.size()));
    return ids[selection];
}
void __stdcall Render() {
    auto view = View();
    if (!Header(view, "Marco Polo - Trade Status"))
        return;
    UI::Text("Reports from %u of %u markets", static_cast<unsigned>(view.known.size()),
             static_cast<unsigned>(view.marketNames.size()));
    auto market = ChooseMarket(view, selected);
    if (market.empty())
        return;
    auto found = view.known.find(market);
    if (found == view.known.end() || found->second.empty()) {
        UI::TextUnformatted("You have no reports from this market.");
        UI::TextUnformatted("Visit its towns or ask an innkeeper for trade news.");
    } else if (UI::BeginTable("Remembered goods", 5,
                              UI::ImGuiTableFlags_RowBg | UI::ImGuiTableFlags_BordersInnerV)) {
        for (const char *label : {"Goods", "Last known rate", "Report date", "Age", "Source"})
            UI::TableSetupColumn(label);
        UI::TableHeadersRow();
        for (auto &[good, name] : view.commodityNames) {
            UI::TableNextRow();
            UI::TableSetColumnIndex(0);
            UI::TextUnformatted(name.c_str());
            UI::TableSetColumnIndex(1);
            auto known = found->second.find(good);
            if (known == found->second.end()) {
                UI::TextUnformatted("Unknown");
                continue;
            }
            auto &r = known->second;
            UI::TextUnformatted(Rate(r).c_str());
            UI::TableSetColumnIndex(2);
            UI::TextUnformatted(view.dates.at(r.observed).c_str());
            UI::TableSetColumnIndex(3);
            UI::TextUnformatted(Age(r.observed, view.day).c_str());
            UI::TableSetColumnIndex(4);
            UI::TextWrapped("%s%s", r.source.c_str(), r.rumor ? " (rumor)" : " (visited)");
            if (r.rumor)
                UI::Text("Heard %s", Age(r.learned, view.day).c_str());
        }
        UI::EndTable();
    }
    UI::Separator();
    UI::TextUnformatted("Possible trade routes");
    if (view.opportunities.empty())
        UI::TextWrapped("Your reports do not yet reveal a large price difference between markets.");
    else {
        UI::TextWrapped("These comparisons use your reports. Prices may have changed since then.");
        for (auto &gap : view.opportunities) {
            UI::TextWrapped("%s: %s (%s) to %s (%s)",
                            view.commodityNames.at(gap.buy.commodity).c_str(),
                            view.marketNames.at(gap.buy.market).c_str(), Rate(gap.buy).c_str(),
                            view.marketNames.at(gap.sell.market).c_str(), Rate(gap.sell).c_str());
            UI::Text("Reports: %s / %s", Age(gap.buy.observed, view.day).c_str(),
                     Age(gap.sell.observed, view.day).c_str());
        }
    }
    UI::Separator();
    UI::TextUnformatted("Recent trade rumors");
    if (view.rumors.empty())
        UI::TextUnformatted("You have not heard any trade rumors yet.");
    for (auto &r : view.rumors) {
        UI::TextWrapped("%s - %s: %s. From %s, heard %s.", view.marketNames.at(r.market).c_str(),
                        view.commodityNames.at(r.commodity).c_str(), Rate(r).c_str(),
                        r.source.c_str(), Age(r.learned, view.day).c_str());
        UI::Text("Report dated %s (%s)", view.dates.at(r.observed).c_str(),
                 Age(r.observed, view.day).c_str());
    }
}
void __stdcall DebugRender() {
    auto view = View();
    if (!Header(view, "Marco Polo - Live markets (debug)") || view.economy.is_null())
        return;
    auto market = ChooseMarket(view, debugSelected);
    if (market.empty())
        return;
    UI::Text("%u shipments in transit", static_cast<unsigned>(view.economy.at("shipments").size()));
    if (UI::BeginTable("Live goods", 3,
                       UI::ImGuiTableFlags_RowBg | UI::ImGuiTableFlags_BordersInnerV)) {
        for (const char *label : {"Goods", "In storage", "Live price index"})
            UI::TableSetupColumn(label);
        UI::TableHeadersRow();
        for (auto &[good, s] : view.economy.at("markets").at(market).at("goods").items()) {
            UI::TableNextRow();
            UI::TableSetColumnIndex(0);
            UI::TextUnformatted(view.commodityNames.at(good).c_str());
            UI::TableSetColumnIndex(1);
            UI::Text("%.0f", s.at("quantity").get<double>());
            UI::TableSetColumnIndex(2);
            UI::Text("%.0f%%", s.at("price").get<double>() * 100);
        }
        UI::EndTable();
    }
    if (UI::Button("Export diagnostic state"))
        SKSE::GetTaskInterface()->AddTask(ExportState);
}
} // namespace
bool RegisterMenu() {
    auto module = GetModuleHandleW(L"SKSEMenuFramework.dll");
    if (!module)
        return false;
    for (const char *name :
         {"AddSectionItem", "igTextUnformatted", "igTextV", "igTextWrappedV", "igCombo_Str_arr",
          "igBeginTable", "igTableSetupColumn", "igTableHeadersRow", "igTableNextRow",
          "igTableSetColumnIndex", "igEndTable", "igSeparator", "igButton"})
        if (!GetProcAddress(module, name))
            return false;
    SKSEMenuFramework::SetSection("Marco Polo");
    SKSEMenuFramework::AddSectionItem("Trade Status", Render);
    if (debugLogging)
        SKSEMenuFramework::AddSectionItem("Live markets (debug)", DebugRender);
    return true;
}
} // namespace marco::runtime
