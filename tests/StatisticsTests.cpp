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
int main(int argc, char **argv) {
    try {
        Check(argc == 2, "Pass economy.json");
        std::ifstream f(argv[1]);
        auto config = Config::Parse(json::parse(f));
        Economy observed(config), plain(config);
        observed.CollectStatistics(true);
        for (int day = 1; day <= 90; ++day) {
            observed.Advance(day);
            plain.Advance(day);
            Check(observed.Save() == plain.Save(), "Telemetry changed the simulation");
            const auto &stats = *observed.Statistics();
            Check(stats.day == day &&
                      stats.demand.size() == config.markets.size() * config.commodities.size(),
                  "Incomplete telemetry");
            for (const auto &d : stats.demand)
                Check(d.wanted >= d.supplied && d.supplied >= 0 && std::isfinite(d.wanted),
                      "Invalid demand telemetry");
            if (day == 47) {
                observed.Load(observed.Save());
                Check(observed.Statistics()->day == -1 && observed.Statistics()->demand.empty(),
                      "Stale telemetry after reload");
            }
        }
        observed.NewGame(0, 1);
        Check(observed.Statistics()->day == -1, "Stale telemetry after new game");
        // A deliberately empty warehouse must report unmet demand even though
        // all ordinary state counters remain finite and the tick succeeds.
        config.routes.clear();
        config.recipes.clear();
        for (auto &m : config.markets)
            m.output.clear();
        Economy empty(config);
        empty.CollectStatistics(true);
        for (auto &[id, m] : empty.Edit().markets)
            for (auto &[good, s] : m.goods)
                s.quantity = 0;
        empty.Advance(1);
        double wanted = 0, supplied = 0;
        for (const auto &d : empty.Statistics()->demand) {
            wanted += d.wanted;
            supplied += d.supplied;
        }
        Check(wanted > 0 && supplied == 0, "Unmet demand was hidden by missing stock");
        empty.CollectStatistics(false);
        empty.Advance(2);
        Check(!empty.Statistics(), "Telemetry remains enabled");
        std::cout
            << "PASS: complete unmet-demand telemetry, state/RNG isolation and lifecycle reset\n";
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
