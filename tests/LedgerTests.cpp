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
        p.scale = 1.4;
        p.locked = true;
        p.pins.insert("drink:rift:pale");
        CHECK(LedgerPreferences::Parse(p.Save()).Save() == p.Save());
        auto clamped =
            LedgerPreferences::Parse({{"x", -10}, {"y", 10}, {"scale", 99}, {"hotkey", 0}});
        CHECK(clamped.x == 0 && clamped.y == 1 && clamped.scale == 1.75 && clamped.hotkey == 0);
        auto invalid =
            LedgerPreferences::Parse({{"x", "bad"},
                                      {"scale", nullptr},
                                      {"locked", "yes"},
                                      {"hotkey", std::numeric_limits<std::uint64_t>::max()},
                                      {"pins", {22, "", "valid", std::string(200, 'a')}}});
        CHECK(invalid.x == .67 && invalid.scale == 1 && !invalid.locked && invalid.hotkey == 0x42);
        CHECK(invalid.pins == std::set<std::string>{"valid"});
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
