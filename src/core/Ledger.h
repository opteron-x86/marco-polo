#pragma once
#include "Knowledge.h"
#include <set>
#include <string_view>

namespace marco {
struct PriceDescription {
    std::string_view text;
    int tone; // -1 cheap, 0 fair, 1 expensive, 2 unknown/uncertain
};
PriceDescription DescribePrice(const TradeReport &report);

// UI preferences are a separate file, never part of a character's economy save.
struct LedgerPreferences {
    double x{.67}, y{.12}, scale{1.0};
    unsigned hotkey{0x0D};         // DirectInput Equals; zero disables the shortcut.
    unsigned hotkeyModifier{0x36}; // Right Shift; zero means no modifier.
    bool locked{};
    std::set<std::string> pins;
    static LedgerPreferences Parse(const json &);
    json Save() const;
};

struct MarketReports {
    std::string market, date;
    double day{};
    std::map<std::string, std::string> marketNames, commodityNames;
    std::map<double, std::string> dates;
    std::map<std::string, std::map<std::string, TradeReport>> known;
    std::vector<PriceGap> opportunities;
    std::vector<TradeReport> rumors;
};
// Only remembered information crosses the UI bridge, already described in words.
json LedgerPayload(const MarketReports &, const LedgerPreferences &);
} // namespace marco
