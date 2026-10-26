#include "core/Economy.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <stdexcept>

using namespace marco;
namespace {
struct Metric {
    const char *market, *good;
};
constexpr Metric metrics[]{{"whiterun", "iron"},  {"eastmarch", "iron"},   {"whiterun", "food"},
                           {"eastmarch", "food"}, {"whiterun", "weapons"}, {"whiterun", "armor"},
                           {"reach", "silver"},   {"pale", "food"},        {"falkreath", "iron"}};
constexpr const char *scenarios[]{"unchanged",          "valtheim_cleared", "all_raiders_cleared",
                                  "crypt_cleared",      "greymoor_cleared", "greymoor_imperial",
                                  "greymoor_stormcloak"};
struct Totals {
    std::map<std::string, double> prices;
    double delivered{}, eastRoadDelivered{}, westRoadDelivered{};
    double departures{}, lossReserved{}, samples{};
};
void Change(Economy &e, const std::string &scenario) {
    auto apply = [&](const char *id, SiteControl control) { e.ObserveSite(id, control, 30); };
    if (scenario == "valtheim_cleared")
        apply("Skyrim.esm|0192C1", SiteControl::Cleared);
    if (scenario == "crypt_cleared")
        apply("Skyrim.esm|018EE9", SiteControl::Cleared);
    if (scenario == "greymoor_cleared")
        apply("Skyrim.esm|01917D", SiteControl::Cleared);
    if (scenario == "greymoor_imperial")
        apply("Skyrim.esm|01917D", SiteControl::Imperial);
    if (scenario == "greymoor_stormcloak")
        apply("Skyrim.esm|01917D", SiteControl::Stormcloak);
    if (scenario == "all_raiders_cleared")
        for (const auto &s : e.Definitions().security.sites)
            if (s.kind != SiteKind::Crypt && s.kind != SiteKind::Unknown)
                e.ObserveSite(s.id, SiteControl::Cleared, 30);
}
} // namespace
int main(int argc, char **argv) {
    try {
        if (argc < 4 || argc > 5) {
            std::cerr
                << "marco_security_sim economy.json security.json output-directory [seeds=32]\n";
            return 2;
        }
        std::ifstream ef(argv[1]), sf(argv[2]);
        auto definitions = json::parse(ef);
        definitions["security"] = json::parse(sf);
        auto config = Config::Parse(definitions);
        unsigned seeds = argc == 5 ? static_cast<unsigned>(std::stoul(argv[4])) : 32;
        if (!seeds || seeds > 1000)
            throw std::runtime_error("Use 1..1000 seeds");
        std::filesystem::path out(argv[3]);
        if (std::filesystem::exists(out) && !std::filesystem::is_empty(out))
            throw std::runtime_error("Use an empty output directory");
        std::filesystem::create_directories(out);
        std::ofstream csv(out / "daily.csv");
        csv << "seed,scenario,day,delivered,departed,loss_reserved,east_road_delivered,west_road_"
               "delivered";
        for (auto m : metrics)
            csv << ',' << m.market << '_' << m.good;
        csv << '\n';
        std::map<std::string, std::map<std::string, Totals>> totals;
        double maxConservationError = 0;
        for (unsigned seed = 1; seed <= seeds; ++seed) {
            Economy warm(config);
            warm.NewGame(0, seed);
            for (const auto &s : config.security.sites)
                if (s.kind != SiteKind::Crypt && s.kind != SiteKind::Unknown)
                    warm.ObserveSite(s.id, SiteControl::Hostile, 0);
            // Match the live runtime's unfinished Civil War conditions.
            for (const auto &m : config.markets)
                warm.SetConditions(m.id, .5,
                                   m.id == "eastmarch" || m.id == "haafingar" ? .15 : .05);
            double initial = 0;
            for (const auto &[id, m] : warm.Get().markets)
                for (const auto &[good, s] : m.goods)
                    initial += s.quantity;
            warm.Advance(30, 30);
            auto common = warm.Save();
            json baseline, imperial;
            for (const auto *scenario : scenarios) {
                Economy e(config);
                e.Load(common);
                Change(e, scenario);
                for (int day = 31; day <= 90; ++day) {
                    double delivered = 0, eastDelivered = 0, westDelivered = 0, departed = 0,
                           losses = 0;
                    for (const auto &s : e.Get().shipments)
                        if (s.arrives <= day) {
                            double quantity = s.quantity * (1 - s.loss);
                            delivered += quantity;
                            if (s.route == "whiterun-eastmarch" || s.route == "eastmarch-whiterun")
                                eastDelivered += quantity;
                            if (s.route == "whiterun-reach" || s.route == "reach-whiterun")
                                westDelivered += quantity;
                        }
                    auto first = e.Get().nextID;
                    e.Advance(day);
                    for (const auto &s : e.Get().shipments)
                        if (s.id >= first) {
                            departed += s.quantity;
                            losses += s.quantity * s.loss;
                        }
                    auto window = day <= 60 ? "days_31_60" : "days_61_90";
                    auto &t = totals[scenario][window];
                    t.delivered += delivered;
                    t.eastRoadDelivered += eastDelivered;
                    t.westRoadDelivered += westDelivered;
                    t.departures += departed;
                    t.lossReserved += losses;
                    ++t.samples;
                    csv << seed << ',' << scenario << ',' << day << ',' << delivered << ','
                        << departed << ',' << losses << ',' << eastDelivered << ','
                        << westDelivered;
                    for (auto m : metrics) {
                        double price = e.Multiplier(m.market, m.good);
                        t.prices[std::string(m.market) + "_" + m.good] += price;
                        csv << ',' << price;
                    }
                    csv << '\n';
                    if (day == 47) {
                        auto saved = e.Save();
                        Economy restored(config);
                        restored.Load(saved);
                        if (restored.Save() != saved)
                            throw std::runtime_error("Round-trip changed state");
                        e = std::move(restored);
                    }
                }
                e.Validate();
                double stock = 0, net = 0;
                for (const auto &[id, m] : e.Get().markets)
                    for (const auto &[good, s] : m.goods) {
                        stock += s.quantity;
                        net += s.produced - s.consumed + s.imported - s.exported - s.lost;
                    }
                maxConservationError =
                    std::max(maxConservationError, std::abs(stock - initial - net));
                auto final = e.Save();
                if (std::string(scenario) == "unchanged")
                    baseline = final;
                if (std::string(scenario) == "crypt_cleared" && final != baseline)
                    throw std::runtime_error("Clearing crypt changed the economy");
                if (std::string(scenario) == "greymoor_imperial")
                    imperial = final;
                if (std::string(scenario) == "greymoor_stormcloak") {
                    final["security"] = imperial["security"];
                    if (final != imperial)
                        throw std::runtime_error("Faction alone changed economic effects");
                }
            }
        }
        json result{{"seeds", seeds},
                    {"warmup_days", 30},
                    {"event_day", 30},
                    {"end_day", 90},
                    {"conditions", "Unfinished Civil War; all configured raider sites initially "
                                   "hostile; no player trades or registered merchants"},
                    {"maximum_stock_accounting_error", maxConservationError},
                    {"scenarios", json::object()}};
        for (const auto &[scenario, windows] : totals)
            for (const auto &[window, t] : windows) {
                auto &j = result["scenarios"][scenario][window];
                for (const auto &[good, price] : t.prices)
                    j["mean_prices"][good] = price / t.samples;
                j["delivered_units_per_day"] = t.delivered / t.samples;
                j["east_road_delivered_per_day"] = t.eastRoadDelivered / t.samples;
                j["west_road_delivered_per_day"] = t.westRoadDelivered / t.samples;
                j["departed_units_per_day"] = t.departures / t.samples;
                j["reserved_loss_fraction"] = t.departures ? t.lossReserved / t.departures : 0;
            }
        std::ofstream summary(out / "summary.json");
        summary << result.dump(2) << '\n';
        csv.close();
        summary.close();
        if (!csv || !summary)
            throw std::runtime_error("Cannot write simulation results");
        if (maxConservationError > 1e-6)
            throw std::runtime_error("Conservation check failed");
        std::cout << seeds << " seeds x 7 scenarios, 30-day warmup + 60-day observation; saved to "
                  << out.string() << '\n';
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
