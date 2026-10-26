#include "core/Ledger.h"
#include <iostream>
#include <limits>
#include <stdexcept>
using namespace marco;
#define CHECK(x)                                                                                   \
    do {                                                                                           \
        if (!(x))                                                                                  \
            throw std::runtime_error(std::string(__FILE__) + ":" + std::to_string(__LINE__) +      \
                                     " " #x);                                                      \
    } while (false)
int main() {
    try {
        TradeReport r{"rift", "drink", "Riften", 2, 2, .5, .5, false};
        for (auto [rate, label, tone] : {std::tuple{.5, "Very cheap", -1},
                                         {.65, "Cheap", -1},
                                         {.85, "Fair", 0},
                                         {1.2, "Fair", 0},
                                         {1.3, "Expensive", 1},
                                         {1.6, "Very expensive", 1}}) {
            r.low = r.high = rate;
            CHECK(DescribePrice(r).text == label);
            CHECK(DescribePrice(r).tone == tone);
        }
        // An interval that crosses fair prices must not promise cheap or dear goods.
        r.rumor = true;
        r.low = .5;
        r.high = .8;
        CHECK(DescribePrice(r).text == "Cheap");
        r.high = 1;
        CHECK(DescribePrice(r).text == "Uncertain");
        r.low = 1.1;
        r.high = 1.8;
        CHECK(DescribePrice(r).text == "Uncertain");
        r.low = 1.3;
        CHECK(DescribePrice(r).text == "Expensive");
        r.low = std::numeric_limits<double>::quiet_NaN();
        CHECK(DescribePrice(r).tone == 2);
        r.low = 3;
        r.high = 2;
        CHECK(DescribePrice(r).tone == 2);
        LedgerPreferences p;
        p.x = .3;
        p.y = .7;
        p.width = 640;
        p.height = 720;
        p.locked = true;
        p.pins.insert("drink:rift:pale");
        CHECK(LedgerPreferences::Parse(p.Save()).Save() == p.Save());
        auto clamped =
            LedgerPreferences::Parse({{"x", -10}, {"y", 10}, {"width", 99999}, {"height", -10}, {"hotkey", 0}});
        CHECK(clamped.x == 0 && clamped.y == 1 && clamped.width == 1600 && clamped.height == 360 && clamped.hotkey == 0);
        auto invalid =
            LedgerPreferences::Parse({{"x", "bad"},
                                      {"width", nullptr},
                                      {"height", std::numeric_limits<double>::infinity()},
                                      {"locked", "yes"},
                                      {"hotkey", std::numeric_limits<std::uint64_t>::max()},
                                      {"pins", {22, "", "valid", std::string(200, 'a')}}});
        CHECK(invalid.x == .67 && invalid.width == 460 && invalid.height == 430 && !invalid.locked && invalid.hotkey == 0x0D);
        CHECK(invalid.pins == std::set<std::string>{"valid"});
        auto scaled = LedgerPreferences::Parse({{"scale", 1.5}, {"x", .25}, {"pins", {"drink:rift:pale"}}});
        CHECK(scaled.width == 690 && scaled.height == 645 && scaled.x == .25);
        CHECK(scaled.pins.contains("drink:rift:pale"));
        CHECK(!scaled.Save().contains("scale"));
        CHECK(LedgerPreferences::Parse(scaled.Save()).Save() == scaled.Save());
        auto explicitSize = LedgerPreferences::Parse({{"scale", 1.5}, {"width", 777}, {"height", 888}});
        CHECK(explicitSize.width == 777 && explicitSize.height == 888);
        auto tooSmall = LedgerPreferences::Parse({{"scale", .65}});
        CHECK(tooSmall.width == 420 && tooSmall.height == 360);
        auto oldDefault =
            LedgerPreferences::Parse({{"hotkey", 0x42}, {"x", .25}, {"locked", true}});
        CHECK(oldDefault.hotkey == 0x0D && oldDefault.hotkeyModifier == 0x36);
        CHECK(oldDefault.x == .25 && oldDefault.locked);
        auto custom = LedgerPreferences::Parse({{"hotkey", 0x43}});
        CHECK(custom.hotkey == 0x43 && custom.hotkeyModifier == 0);
        auto chord = LedgerPreferences::Parse({{"hotkey", 0x0D}, {"hotkeyModifier", 0x36}});
        CHECK(LedgerPreferences::Parse(chord.Save()).Save() == chord.Save());
        auto badModifier = LedgerPreferences::Parse({{"hotkeyModifier", 256}});
        CHECK(badModifier.hotkeyModifier == 0x36);
        // Escape must remain distinct from Tab regardless of Skyrim's UI key translation.
        CHECK(LedgerKeyCommand(0x01) == "close" && LedgerKeyCommand(0x01, true) == "close");
        CHECK(LedgerKeyCommand(0x0F) == "nextTab" && LedgerKeyCommand(0x0F, true) == "prevTab");
        CHECK(LedgerKeyCommand(0x1B).empty() && LedgerKeyCommand(0x09).empty());
        CHECK(LedgerKeyCommand(0x0D).empty()); // Opening chord is handled separately.
        CHECK(LedgerKeyCommand(0x02) == "markets" && LedgerKeyCommand(0x03) == "routes" &&
              LedgerKeyCommand(0x04) == "rumors");
        CHECK(LedgerKeyCommand(0xCB) == "left" && LedgerKeyCommand(0xCD) == "right");
        CHECK(LedgerKeyCommand(0xC9) == "up" && LedgerKeyCommand(0xD1) == "down");
        bool rejected = false;
        try {
            LedgerPreferences::Parse({{"schema", 7}});
        } catch (const std::exception &) {
            rejected = true;
        }
        CHECK(rejected);
        MarketReports v;
        v.market = "pale";
        v.day = 10;
        v.date = "10th of Last Seed";
        v.marketNames = {{"rift", "The Rift"}, {"pale", "The Pale"}, {"reach", "The Reach"}};
        v.commodityNames = {{"drink", "Drink"}, {"iron", "Iron"}};
        v.dates = {{2, "2nd of Last Seed"}, {9, "9th of Last Seed"}};
        r = {"rift", "drink", "Riften", 2, 2, .5, .5, false};
        TradeReport rumor{"pale", "drink", "Hulda", 2, 9, 1.5, 2, true};
        v.known["rift"]["drink"] = r;
        v.known["pale"]["drink"] = rumor;
        v.rumors.push_back(rumor);
        v.opportunities.push_back({r, rumor, 3});
        auto before = v.known;
        auto payload = LedgerPayload(v, p);
        CHECK(payload["preferences"]["width"] == 640 && payload["preferences"]["height"] == 720);
        CHECK(!payload["preferences"].contains("scale"));
        CHECK(payload["markets"].size() == 3);
        CHECK(payload["markets"][1]["id"] == "reach");
        for (auto &row : payload["markets"][1]["rows"])
            CHECK(row["price"] == "Unknown");
        CHECK(payload["markets"][2]["rows"][0]["price"] == "Very cheap");
        CHECK(payload["markets"][2]["rows"][0]["age"] == "8d old");
        CHECK(payload["markets"][2]["rows"][1]["price"] == "Unknown");
        CHECK(payload["routes"][0]["pinned"] == true);
        CHECK(payload["routes"][0]["buy"]["age"] == "8d old");
        CHECK(payload["rumors"][0]["age"] == "8d old");
        CHECK(payload["rumors"][0]["detail"].get<std::string>().find("Heard 9th of Last Seed") !=
              std::string::npos);
        // Formatting cannot change knowledge; precision never crosses into the player UI.
        CHECK(v.known.at("rift").at("drink").low == before.at("rift").at("drink").low);
        CHECK(v.known.at("rift").at("drink").observed == 2);
        for (auto forbidden : {"\"ratio\"", "\"low\"", "\"high\"", "\"quantity\"", "%"})
            CHECK(payload.dump().find(forbidden) == std::string::npos);
        CHECK(!payload["preferences"].contains("pins"));
        MarketReports other;
        other.marketNames = v.marketNames;
        other.commodityNames = v.commodityNames;
        auto fresh = LedgerPayload(other, p);
        CHECK(fresh["routes"].empty() && fresh["rumors"].empty());
        for (auto &market : fresh["markets"])
            for (auto &row : market["rows"])
                CHECK(row["price"] == "Unknown");
        std::cout
            << "Ledger uncertainty, preferences, report ages and information isolation passed\n";
        return 0;
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
