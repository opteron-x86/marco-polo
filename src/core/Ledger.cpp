#include "Ledger.h"
#include <algorithm>
#include <cmath>
#include <format>
#include <stdexcept>

namespace marco {
PriceDescription DescribePrice(const TradeReport &r) {
    if (!std::isfinite(r.low) || !std::isfinite(r.high) || r.low < 0 || r.high < r.low)
        return {"Uncertain", 2};
    auto band = [](double n) { return n < .65 ? 0 : n < .85 ? 1 : n <= 1.2 ? 2 : n < 1.6 ? 3 : 4; };
    int low = band(r.low), high = band(r.high);
    int value = low == high ? low : high <= 1 ? 1 : low >= 3 ? 3 : -1;
    constexpr std::string_view names[]{"Very cheap", "Cheap", "Fair", "Expensive",
                                       "Very expensive"};
    return value < 0 ? PriceDescription{"Uncertain", 2}
                     : PriceDescription{names[value], value < 2   ? -1
                                                      : value > 2 ? 1
                                                                  : 0};
}
LedgerPreferences LedgerPreferences::Parse(const json &j) {
    if (!j.is_object() || j.value("schema", 1) != 1)
        throw std::runtime_error("Invalid ledger preferences schema");
    LedgerPreferences p;
    auto number = [&](const char *name, double fallback, double min, double max) {
        auto it = j.find(name);
        if (it == j.end() || !it->is_number())
            return fallback;
        double n = it->get<double>();
        return std::isfinite(n) ? std::clamp(n, min, max) : fallback;
    };
    p.x = number("x", p.x, 0, 1);
    p.y = number("y", p.y, 0, 1);
    p.scale = number("scale", p.scale, .65, 1.75);
    if (j.contains("hotkey") && j["hotkey"].is_number_integer()) {
        auto code = j["hotkey"].get<double>();
        if (code >= 0 && code <= 255)
            p.hotkey = static_cast<unsigned>(code);
    }
    if (j.contains("locked") && j["locked"].is_boolean())
        p.locked = j["locked"].get<bool>();
    if (j.contains("pins") && j["pins"].is_array())
        for (auto &pin : j["pins"]) {
            if (p.pins.size() >= 64)
                break;
            if (pin.is_string()) {
                auto key = pin.get<std::string>();
                if (!key.empty() && key.size() <= 192)
                    p.pins.insert(std::move(key));
            }
        }
    return p;
}
json LedgerPreferences::Save() const {
    return {{"schema", 1},      {"x", x},           {"y", y},      {"scale", scale},
            {"hotkey", hotkey}, {"locked", locked}, {"pins", pins}};
}
json LedgerPayload(const MarketReports &v, const LedgerPreferences &p) {
    auto name = [](const auto &names, const std::string &id) {
        auto found = names.find(id);
        return found == names.end() ? id : found->second;
    };
    auto describe = [&](const TradeReport &r) {
        auto price = DescribePrice(r);
        double age = std::max(0.0, std::floor(v.day) - std::floor(r.observed));
        std::string shortAge =
            age < 1 ? "Today" : std::format("{:.0f}d{}", age, age >= 7 ? " old" : "");
        auto date = v.dates.find(r.observed);
        std::string detail = name(v.marketNames, r.market) + " / " +
                             name(v.commodityNames, r.commodity) + "\n" + std::string(price.text) +
                             "\n" + r.source + (r.rumor ? " (rumor)" : " (visited)");
        if (date != v.dates.end())
            detail += "\n" + date->second;
        if (r.rumor) {
            auto heard = v.dates.find(r.learned);
            if (heard != v.dates.end())
                detail += "\nHeard " + heard->second;
        }
        if (age >= 7)
            detail += "\nOld report";
        return json{{"name", name(v.commodityNames, r.commodity)},
                    {"market", name(v.marketNames, r.market)},
                    {"price", price.text},
                    {"tone", price.tone},
                    {"age", shortAge},
                    {"detail", detail}};
    };
    json out{{"current", v.market},      {"date", v.date},          {"preferences", p.Save()},
             {"markets", json::array()}, {"routes", json::array()}, {"rumors", json::array()}};
    // Pins are keys, never reports. A different character still sees only their own knowledge.
    out["preferences"].erase("pins");
    for (auto &[id, marketName] : v.marketNames) {
        json rows = json::array();
        auto market = v.known.find(id);
        for (auto &[good, goodName] : v.commodityNames) {
            if (market != v.known.end()) {
                auto report = market->second.find(good);
                if (report != market->second.end()) {
                    rows.push_back(describe(report->second));
                    continue;
                }
            }
            rows.push_back({{"name", goodName},
                            {"price", "Unknown"},
                            {"tone", 2},
                            {"age", ""},
                            {"detail", marketName + " / " + goodName + "\nNo report"}});
        }
        out["markets"].push_back({{"id", id}, {"name", marketName}, {"rows", rows}});
    }
    for (auto &gap : v.opportunities) {
        auto buy = describe(gap.buy), sell = describe(gap.sell);
        std::string key = gap.buy.commodity + ":" + gap.buy.market + ":" + gap.sell.market;
        out["routes"].push_back({{"key", key},
                                 {"pinned", p.pins.contains(key)},
                                 {"name", name(v.commodityNames, gap.buy.commodity)},
                                 {"buy", buy},
                                 {"sell", sell},
                                 {"detail", buy["detail"].get<std::string>() + "\n\n" +
                                                sell["detail"].get<std::string>()}});
    }
    for (auto &r : v.rumors)
        out["rumors"].push_back(describe(r));
    return out;
}
} // namespace marco
