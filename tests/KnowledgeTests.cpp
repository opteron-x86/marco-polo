#include "core/Knowledge.h"
#include <cmath>
#include <fstream>
#include <iostream>
#include <stdexcept>
using namespace marco;
#define CHECK(x)                                                                                   \
    do {                                                                                           \
        if (!(x))                                                                                  \
            throw std::runtime_error(std::string(__FILE__) + ":" + std::to_string(__LINE__) +      \
                                     " " #x);                                                      \
    } while (false)
template <class F> void Rejects(F f) {
    bool rejected = false;
    try {
        f();
    } catch (const std::exception &) {
        rejected = true;
    }
    CHECK(rejected);
}
int main(int argc, char **argv) {
    try {
        CHECK(argc == 2);
        std::ifstream file(argv[1]);
        Config config = Config::Parse(json::parse(file));
        Economy e(config), control(config);
        e.NewGame(1.25, 42);
        control.NewGame(1.25, 42);
        Knowledge k;
        k.Capture(e);
        CHECK(k.Reports().empty());
        CHECK(k.Opportunities().empty());
        k.Observe(e, "rift", 1.5, "Riften");
        auto memory = k.Save().at("reports");
        auto rate = k.Reports().at("rift").at("drink").low;
        CHECK(k.Reports().size() == 1);
        CHECK(k.Reports().at("rift").size() == config.commodities.size());
        CHECK(!k.Reports().at("rift").at("drink").rumor);
        // Updating the world and rendering/exporting information cannot reveal
        // or refresh markets the player has not visited.
        k.Advance(e, 12.5, 32);
        control.Advance(12.5, 32);
        CHECK(e.Save() == control.Save());
        CHECK(k.Save().at("reports") == memory);
        CHECK(k.Reports().at("rift").at("drink").low == rate);
        CHECK(!k.Reports().contains("whiterun"));
        k.Observe(e, "pale", 12.5, "Dawnstar");
        CHECK(k.Reports().at("rift").at("drink").observed == 1.5);
        CHECK(k.Reports().at("pale").at("drink").observed == 12.5);
        // Barter pauses game time: successive confirmed trades must still
        // refresh a firsthand report at the very same calendar timestamp.
        e.Edit().markets.at("pale").goods.at("drink").price = 2.5;
        k.Observe(e, "pale", 12.5, "Thoring");
        CHECK(k.Reports().at("pale").at("drink").low == 2.5);
        CHECK(k.Reports().at("pale").at("drink").source == "Thoring");
        CHECK(!k.Opportunities().empty());
        auto gaps = k.Opportunities();
        e.Edit().markets.at("winterhold").goods.at("drink").price = 3;
        CHECK(k.Opportunities().front().ratio == gaps.front().ratio);
        // Rumors must be backed by earlier samples along incoming trade routes.
        auto batch = k.Hear(e, "whiterun", 12.5, "Hulda");
        CHECK(batch.reports.size() == 3);
        auto after = k.Save();
        for (auto &r : batch.reports) {
            CHECK(r.rumor && r.market != "whiterun");
            double delay = 1000;
            for (auto &route : config.routes)
                if (route.to == "whiterun" && route.from == r.market)
                    delay = std::min(delay, std::max(1.0, route.days));
            CHECK(r.observed <= 12.5 - delay);
            bool backed = false;
            for (auto &sample : after.at("history"))
                if (sample.at("day") == r.observed) {
                    double actual = sample.at("rates").at(r.market).at(r.commodity);
                    CHECK(r.low <= actual + .00001 && r.high >= actual - .00001);
                    backed = true;
                }
            CHECK(backed);
            CHECK(Knowledge::RumorText(r, config).find(config.Market(r.market).name) !=
                  std::string::npos);
        }
        k.Hear(e, "whiterun", 12.9, "Another innkeeper");
        CHECK(k.Save() == after);
        Knowledge restored;
        restored.Load(json::parse(after.dump()), config, 12.9);
        CHECK(restored.Save() == after);
        restored.Hear(e, "whiterun", 12.9, "Hulda");
        CHECK(restored.Save() == after);
        // Stale rumors cannot overwrite newer firsthand reports.
        Knowledge firsthand;
        for (auto &market : config.markets)
            firsthand.Observe(e, market.id, 12.5, "Visited");
        auto firstReports = firsthand.Save().at("reports");
        firsthand.Hear(e, "whiterun", 12.5, "Hulda");
        CHECK(firsthand.Save().at("reports") == firstReports);
        for (int corruption = 0; corruption < 6; ++corruption) {
            auto invalid = after;
            auto &r = invalid["reports"]["rift"]["drink"];
            switch (corruption) {
            case 0:
                invalid["schema"] = 9;
                break;
            case 1:
                r["observed"] = 999;
                break;
            case 2:
                r["low"] = -1;
                break;
            case 3:
                r["market"] = "missing";
                break;
            case 4:
                invalid["history"][0]["rates"]["rift"].erase("drink");
                break;
            case 5:
                invalid["batches"]["whiterun"]["reports"][0]["learned"] = 0;
                break;
            }
            Rejects([&] { restored.Load(invalid, config, 12.9); });
            CHECK(restored.Save() == after);
        }
        // Catch-up batching must not change either the world or news history.
        Knowledge a, b;
        Economy ea(config), eb(config);
        ea.NewGame(0, 99);
        eb.NewGame(0, 99);
        a.Advance(ea, 80, 100);
        while (b.Advance(eb, 80, 3)) {
        }
        CHECK(ea.Save() == eb.Save());
        CHECK(a.Save() == b.Save());
        CHECK(a.Save().at("history").size() == Knowledge::HistoryLimit);
        a.Load(a.Save(), config, 80);
        Knowledge fresh;
        Economy newGame(config);
        newGame.NewGame(100, 1);
        auto first = fresh.Hear(newGame, "whiterun", 100, "Hulda");
        CHECK(!first.reports.empty());
        for (auto &r : first.reports)
            CHECK(r.market == "whiterun" && r.observed == 100);
        fresh.Reset();
        CHECK(fresh.Reports().empty() && fresh.RecentRumors().empty());
        CHECK(fresh.Save().at("history").empty());
        std::cout << "Remembered markets, delayed rumors, persistence and isolation passed\n";
        return 0;
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
