#include "core/Economy.h"
#include <cmath>
#include <fstream>
#include <iostream>
#include <stdexcept>

using namespace marco;
void Check(bool value, const char *message) {
    if (!value)
        throw std::runtime_error(message);
}
Config Fixture(double reserves = 0) {
    Config c;
    c.balancedSeasons = true;
    c.commodities = {{"grain", "Grain", 10, 0, 0, true, false, 1, reserves}};
    c.markets = {{"hold", "Hold", "", 1, .8, {{"grain", 12}}, {{"grain", 10}}, {}}};
    return c;
}
template <class F> void Rejects(F f) {
    bool rejected = false;
    try {
        f();
    } catch (const std::exception &) {
        rejected = true;
    }
    Check(rejected, "Invalid seasonal configuration accepted");
}
int main(int argc, char **argv) {
    try {
        Check(argc == 2, "Pass economy.json");
        // With enough consumption to use all output, climate redistributes the
        // year's production without reducing its annual productive potential.
        auto c = Fixture();
        c.markets.front().output["grain"] = 10;
        c.markets.front().demand["grain"] = 1000;
        auto mild = c;
        mild.markets.front().winterSeverity = .2;
        Economy cold(c), temperate(mild);
        for (auto *e : {&cold, &temperate})
            e->Edit().markets.at("hold").goods.at("grain").quantity = 0;
        double coldWinter = 0, mildWinter = 0, coldSummer = 0, mildSummer = 0;
        for (int day = 1; day <= 365; ++day) {
            double a = cold.Get().markets.at("hold").goods.at("grain").produced;
            double b = temperate.Get().markets.at("hold").goods.at("grain").produced;
            cold.Advance(day);
            temperate.Advance(day);
            if (day == 138) {
                coldWinter = cold.Get().markets.at("hold").goods.at("grain").produced - a;
                mildWinter = temperate.Get().markets.at("hold").goods.at("grain").produced - b;
            }
            if (day == 320) {
                coldSummer = cold.Get().markets.at("hold").goods.at("grain").produced - a;
                mildSummer = temperate.Get().markets.at("hold").goods.at("grain").produced - b;
            }
        }
        for (auto *e : {&cold, &temperate})
            Check(std::abs(e->Get().markets.at("hold").goods.at("grain").produced -
                           10 * 1.5 * 365) < 1e-7,
                  "Seasonal curve changed annual production potential");
        Check(coldWinter < mildWinter && coldSummer > mildSummer,
              "Regional climate failed to change seasonal production");
        // Preparation must peak before winter, not after it. A weather-insensitive
        // producer isolates the reserve plan from the production curve itself.
        c = Fixture(40);
        c.commodities.front().productionSwing = 0;
        Economy autumn(c), spring(c);
        autumn.NewGame(46, 1);
        spring.NewGame(229, 1);
        for (auto *e : {&autumn, &spring})
            e->Edit().markets.at("hold").goods.at("grain").quantity = 200;
        autumn.Advance(47);
        spring.Advance(230);
        Check(autumn.Get().markets.at("hold").goods.at("grain").produced > 10 &&
                  spring.Get().markets.at("hold").goods.at("grain").produced < 1,
              "Reserves are built in the wrong season");
        // Storage changes procurement and production, not starting inventory or
        // the reference stock used for trade quotes.
        c = Fixture(60);
        auto noReserve = c;
        noReserve.commodities.front().reserveDays = 0;
        Economy stored(c), ordinary(noReserve);
        Check(stored.Save() == ordinary.Save(), "Planning created free starting stock");
        Check(stored.Price("hold", "grain", 10, 5, true).total ==
                  ordinary.Price("hold", "grain", 10, 5, true).total,
              "Planning alone altered the trade quote");
        stored.CollectStatistics(true);
        ordinary.CollectStatistics(true);
        double storedUnmet = 0, ordinaryUnmet = 0, maximum = 0;
        for (int day = 1; day <= 3 * 365; ++day) {
            stored.Advance(day);
            ordinary.Advance(day);
            maximum = std::max(maximum, stored.Get().markets.at("hold").goods.at("grain").quantity);
            if (day > 730) {
                const auto &a = stored.Statistics()->demand.front();
                const auto &b = ordinary.Statistics()->demand.front();
                storedUnmet += a.wanted - a.supplied;
                ordinaryUnmet += b.wanted - b.supplied;
            }
        }
        Check(ordinaryUnmet > 0 && storedUnmet < ordinaryUnmet * .5,
              "Seasonal reserves failed to bridge the lean period");
        Check(maximum < 1200, "Seasonal warehouse accumulation is unbounded");
        const auto &s = stored.Get().markets.at("hold").goods.at("grain");
        Check(std::abs(100 + s.produced - s.consumed - s.lost - s.quantity) < 1e-7,
              "Seasonal storage created or lost goods");
        // Perishables cannot be assigned a long-lived commodity's huge reserve.
        c.commodities.front().shelfLife = 80;
        c.commodities.front().reserveDays = 180;
        auto capped = c;
        capped.commodities.front().reserveDays = 20;
        Economy oversized(c), sensible(capped);
        for (int day = 1; day <= 365; ++day) {
            oversized.Advance(day);
            sensible.Advance(day);
        }
        Check(oversized.Save() == sensible.Save(), "Perishable reserve ignored shelf life");
        // A seasonal order must not lock an exporter's entire buffer away from
        // its trading partners. The ordinary local export reserve still applies.
        c = Fixture(120);
        c.markets.front().output.clear();
        auto destination = c.markets.front();
        destination.id = destination.name = "destination";
        c.markets.push_back(destination);
        c.routes = {{"road", "hold", "destination", 2, 100, 0, 0}};
        Economy exporting(c);
        exporting.NewGame(46, 7);
        exporting.Edit().markets.at("hold").goods.at("grain").quantity = 110;
        exporting.Edit().markets.at("destination").goods.at("grain").quantity = 0;
        exporting.Advance(47);
        Check(!exporting.Get().shipments.empty() && exporting.Get().shipments.front().quantity > 30,
              "Seasonal reserve prevented available exports");
        Check(exporting.Get().markets.at("hold").goods.at("grain").quantity >= 60 - 1e-8,
              "Seasonal orders consumed the exporter's local reserve");
        // Updating the policy preserves saved quantities, prices and cargo.
        auto oldPolicy = c;
        oldPolicy.balancedSeasons = false;
        Economy previous(oldPolicy);
        previous.Load(exporting.Save());
        auto saved = previous.Save();
        Economy updated(c), batched(c);
        updated.Load(saved);
        batched.Load(saved);
        Check(updated.Save() == saved, "Policy update replaced saved trading state");
        while (updated.Advance(500, 1)) {
        }
        while (batched.Advance(500, 29)) {
        }
        Check(updated.Save() == batched.Save(), "Seasonal catch-up depends on tick batching");
        std::ifstream file(argv[1]);
        auto data = json::parse(file);
        data["policy"].erase("balancedSeasons");
        for (auto &good : data["commodities"]) {
            good.erase("productionSwing");
            good.erase("reserveDays");
        }
        auto legacy = Config::Parse(data);
        Check(!legacy.balancedSeasons && legacy.commodities.front().reserveDays == 0,
              "Legacy configuration changed seasonal policy");
        for (double invalid : {-1.0, 181.0, std::nan("")}) {
            auto bad = c;
            bad.commodities.front().reserveDays = invalid;
            Rejects([&] { Economy rejected(bad); });
        }
        for (double invalid : {-1.0, 1.01, std::nan("")}) {
            auto bad = c;
            bad.commodities.front().productionSwing = invalid;
            Rejects([&] { Economy rejected(bad); });
        }
        std::cout << "PASS: annual supply, reserve timing, lean-season coverage, perishable "
                     "limits, exports, conservation, policy upgrades and catch-up\n";
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
