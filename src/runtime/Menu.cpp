#include "Runtime.h"
#include "SKSEMenuFramework.h"
#include <algorithm>

namespace marco::runtime {
namespace {
namespace UI = ImGuiMCP;
int selected = 0;
void __stdcall Render() {
    auto view = View();
    UI::TextUnformatted("Marco Polo — A Skyrim Economy Mod");
    if (!view.ready) {
        UI::TextWrapped("%s", view.status.c_str());
        UI::TextUnformatted("Load a game to see its markets.");
        return;
    }
    auto &markets = view.economy.at("markets");
    std::vector<std::string> ids;
    for (auto &[id, v] : markets.items())
        ids.push_back(id);
    std::vector<const char *> names;
    for (auto &id : ids)
        names.push_back(
            view.economy.at("marketNames").at(id).get_ref<const std::string &>().c_str());
    selected = std::clamp(selected, 0, static_cast<int>(ids.size()) - 1);
    UI::Combo("Market", &selected, names.data(), static_cast<int>(names.size()));
    auto &market = markets.at(ids[selected]);
    UI::Text("%s | %u shipments in transit",
             view.economy.at("date").get_ref<const std::string &>().c_str(),
             static_cast<unsigned>(view.economy.at("shipments").size()));
    if (UI::BeginTable("Goods", 3, UI::ImGuiTableFlags_RowBg | UI::ImGuiTableFlags_BordersInnerV)) {
        UI::TableSetupColumn("Goods");
        UI::TableSetupColumn("In storage");
        UI::TableSetupColumn("Price index");
        UI::TableHeadersRow();
        for (auto &[name, s] : market.at("goods").items()) {
            UI::TableNextRow();
            UI::TableSetColumnIndex(0);
            UI::TextUnformatted(
                view.economy.at("commodityNames").at(name).get_ref<const std::string &>().c_str());
            UI::TableSetColumnIndex(1);
            UI::Text("%.0f", s.at("quantity").get<double>());
            UI::TableSetColumnIndex(2);
            UI::Text("%.0f%%", s.at("price").get<double>() * 100);
        }
        UI::EndTable();
    }
    UI::Separator();
    if (debugLogging && UI::Button("Export diagnostic state"))
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
    SKSEMenuFramework::AddSectionItem("Market ledger", Render);
    return true;
}
} // namespace marco::runtime
