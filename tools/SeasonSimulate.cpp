#include "core/Economy.h"
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <stdexcept>

using namespace marco;
namespace {
using Key = std::pair<std::string, std::string>;
void Check(bool condition, const char *message) {
    if (!condition)
        throw std::runtime_error(message);
}
void Shops(Economy &e) {
    for (const auto &market : e.Definitions().markets) {
        std::map<std::string, Merchant> shops;
        for (const auto &good : e.Definitions().commodities) {
            std::string kind = "smith";
            if (good.id == "grain" || good.id == "food" || good.id == "meat" || good.id == "drink")
                kind = "provisions";
            else if (good.id == "ingredients" || good.id == "medicine")
                kind = "apothecary";
            else if (good.id == "books" || good.id == "housewares")
                kind = "general";
            else if (good.id == "silver" || good.id == "gold" || good.id == "gems" ||
                     good.id == "soul_gems")
                kind = "jeweler";
            auto id = market.id + ':' + kind;
            auto &shop = shops[id];
            shop.id = id;
            shop.market = market.id;
            shop.cash = 2000;
            int target = good.id == "weapons" || good.id == "armor"    ? 8
                         : good.id == "gems" || good.id == "soul_gems" ? 6
                                                                       : 20;
            for (int v = 0; v < 2; ++v) {
                auto form = good.id + (v ? ":b" : ":a");
                int n = v ? target / 3 : target - target / 3;
                shop.items[form] = {form, good.id, n, n, 1, good.referenceValue * (v ? 1.5 : .75),
                                    false};
            }
        }
        for (auto &[id, shop] : shops)
            e.RegisterMerchant(std::move(shop));
    }
}
struct Metric {
    double days{}, wanted{}, supplied{}, produced{}, consumed{}, spoiled{}, transitLoss{};
    double imports{}, exports{}, stock{}, price{}, ceiling{}, floor{}, offers{}, empty{};
    double lowStock{}, minStock{1e100}, maxStock{};
};
struct Road {
    double days{}, capacity{}, shipped{}, full{};
};
void Write(std::ostream &out, const std::string &prefix, int period,
           const std::map<Key, Metric> &metrics) {
    for (const auto &[key, m] : metrics)
        out << prefix << ',' << period << ',' << key.first << ',' << key.second << ',' << m.days
            << ',' << m.wanted << ',' << m.supplied << ',' << m.produced << ',' << m.consumed << ','
            << m.spoiled << ',' << m.transitLoss << ',' << m.imports << ',' << m.exports << ','
            << m.stock / m.days << ',' << m.minStock << ',' << m.maxStock << ',' << m.price / m.days
            << ',' << m.ceiling / m.days << ',' << m.floor / m.days << ','
            << (m.offers ? m.empty / m.offers : 0) << ',' << m.lowStock / m.days << '\n';
}
void Conditions(Economy &e, const std::string &scenario, int day) {
    bool unrest = scenario == "unrest";
    int phase = (day - 1) % 365;
    bool shock = day > 730 && day <= 1095 &&
                 ((scenario == "winter_disruption" && phase >= 107 && phase < 167) ||
                  (scenario == "summer_disruption" && phase >= 289 && phase < 349));
    for (const auto &m : e.Definitions().markets) {
        double disruption = unrest ? (m.id == "haafingar" || m.id == "eastmarch" ? .15 : .05) : 0;
        if (shock && (m.id == "winterhold" || m.id == "pale" || m.id == "hjaalmarch"))
            disruption = .6;
        e.SetConditions(m.id, unrest ? .5 : 0, disruption);
    }
    if (day == 1)
        for (const auto &s : e.Definitions().security.sites) {
            if (s.kind == SiteKind::Crypt || s.kind == SiteKind::Unknown)
                continue;
            SiteControl control =
                s.kind == SiteKind::Fort ? SiteControl::Imperial : SiteControl::Cleared;
            if (unrest)
                control = s.kind == SiteKind::Fort && s.initialOccupants == SiteKind::Unknown
                              ? SiteControl::Unknown
                              : SiteControl::Hostile;
            e.ObserveSite(s.id, control, 0);
        }
}
} // namespace
int main(int argc, char **argv) {
    try {
        Check(argc >= 4 && argc <= 8, "marco_season_sim economy.json security.json output "
                                      "[seeds=8] [years=5] [first-seed=1] [scenario=all]");
        std::ifstream input(argv[1]), security(argv[2]);
        auto data = json::parse(input);
        data["security"] = json::parse(security);
        auto config = Config::Parse(data);
        unsigned seeds = argc > 4 ? std::stoul(argv[4]) : 8;
        int years = argc > 5 ? std::stoi(argv[5]) : 5;
        unsigned first = argc > 6 ? std::stoul(argv[6]) : 1;
        std::string selected = argc > 7 ? argv[7] : "all";
        const std::vector<std::string> scenarios{"peace", "unrest", "winter_disruption",
                                                 "summer_disruption"};
        Check(seeds && seeds <= 256 && first && first <= 10000 && years >= 1 && years <= 10,
              "Invalid seasonal experiment dimensions");
        Check(selected == "all" ||
                  std::find(scenarios.begin(), scenarios.end(), selected) != scenarios.end(),
              "Unknown seasonal scenario");
        std::filesystem::path dir(argv[3]);
        Check(!std::filesystem::exists(dir) || std::filesystem::is_empty(dir),
              "Use an empty output directory");
        std::filesystem::create_directories(dir);
        std::ofstream annual(dir / "annual.csv"), monthly(dir / "monthly.csv"),
            roads(dir / "roads.csv");
        const char *header =
            "seed,scenario,period,market,good,days,wanted,supplied,produced,consumed,spoiled,"
            "transit_loss,imports,exports,mean_stock,min_stock,max_stock,mean_price,ceiling_"
            "fraction,floor_fraction,empty_fraction,low_stock_fraction\n";
        annual << header;
        monthly << header;
        roads << "seed,scenario,year,route,capacity,shipped,full_fraction\n";
        for (auto *f : {&annual, &monthly, &roads})
            *f << std::setprecision(12);
        double maxError = 0;
        std::size_t maxSave = 0;
        for (unsigned seed = first; seed < first + seeds; ++seed) {
            for (const auto &scenario : scenarios) {
                if (selected != "all" && selected != scenario)
                    continue;
                Economy e(config);
                e.NewGame(0, seed);
                e.CollectStatistics(true);
                std::map<std::string, double> initial;
                for (const auto &[id, m] : e.Get().markets)
                    for (const auto &[good, s] : m.goods)
                        initial[good] += s.quantity;
                Shops(e);
                std::map<Key, Metric> year, month;
                std::map<std::string, Road> usage;
                auto prefix = std::to_string(seed) + ',' + scenario;
                for (int day = 1; day <= years * 365; ++day) {
                    Conditions(e, scenario, day);
                    auto previous = e.Get().markets;
                    auto nextID = e.Get().nextID;
                    std::map<Key, double> transit;
                    for (const auto &s : e.Get().shipments)
                        if (s.arrives <= day)
                            transit[{s.to, s.commodity}] += s.quantity * s.loss;
                    e.Advance(day);
                    std::map<Key, std::pair<int, int>> offers;
                    std::map<Key, double> shelf;
                    for (const auto &[id, shop] : e.Get().merchants)
                        for (const auto &[form, item] : shop.items) {
                            auto key = Key{shop.market, item.commodity};
                            ++offers[key].first;
                            offers[key].second += item.quantity == 0;
                            shelf[key] += (item.quantity + item.externalQuantity) * item.units;
                        }
                    for (const auto &d : e.Statistics()->demand) {
                        auto key = Key{d.market, d.commodity};
                        const auto &s = e.Get().markets.at(d.market).goods.at(d.commodity);
                        const auto &old = previous.at(d.market).goods.at(d.commodity);
                        for (auto *metrics : {&year, &month}) {
                            auto &m = (*metrics)[key];
                            ++m.days;
                            m.wanted += d.wanted;
                            m.supplied += d.supplied;
                            m.produced += s.produced - old.produced;
                            m.consumed += s.consumed - old.consumed;
                            m.spoiled += s.lost - old.lost - transit[key];
                            m.transitLoss += transit[key];
                            m.imports += s.imported - old.imported;
                            m.exports += s.exported - old.exported;
                            m.stock += s.quantity;
                            m.minStock = std::min(m.minStock, s.quantity);
                            m.maxStock = std::max(m.maxStock, s.quantity);
                            m.price += s.price;
                            m.ceiling += s.price >= config.maxMultiplier - 1e-8;
                            m.floor += s.price <= config.minMultiplier + 1e-8;
                            m.offers += offers[key].first;
                            m.empty += offers[key].second;
                            m.lowStock += s.quantity + shelf[key] < d.wanted;
                        }
                    }
                    std::map<std::string, double> cargo;
                    for (const auto &s : e.Get().shipments)
                        if (s.id >= nextID)
                            cargo[s.route] += s.quantity;
                    auto security = e.SecurityConditions();
                    for (const auto &r : config.routes) {
                        double disruption = std::max(e.Get().markets.at(r.from).disruption,
                                                     e.Get().markets.at(r.to).disruption);
                        double capacity =
                            r.capacity * (1 - disruption * .75) * security.routes[r.id].capacity;
                        auto &u = usage[r.id];
                        ++u.days;
                        u.capacity += capacity;
                        u.shipped += cargo[r.id];
                        u.full += cargo[r.id] >= capacity - 1e-7;
                    }
                    if (day % 30 == 0 || day == years * 365) {
                        Write(monthly, prefix, (day - 1) / 30 + 1, month);
                        month.clear();
                        auto residual = initial;
                        for (const auto &[id, m] : e.Get().markets)
                            for (const auto &[good, s] : m.goods)
                                residual[good] += s.produced - s.consumed + s.imported -
                                                  s.exported - s.lost - s.quantity;
                        for (const auto &[id, shop] : e.Get().merchants)
                            for (const auto &[form, item] : shop.items)
                                residual[item.commodity] -=
                                    (item.quantity + item.externalQuantity) * item.units;
                        for (const auto &[good, error] : residual)
                            maxError = std::max(maxError, std::abs(error));
                        Check(maxError < 1e-5, "Seasonal stock accounting failed");
                        e.Validate();
                    }
                    if (day % 365 == 0) {
                        Write(annual, prefix, day / 365, year);
                        year.clear();
                        for (const auto &[id, u] : usage)
                            roads << prefix << ',' << day / 365 << ',' << id << ',' << u.capacity
                                  << ',' << u.shipped << ',' << u.full / u.days << '\n';
                        usage.clear();
                        auto save = e.Save();
                        maxSave = std::max(maxSave, save.dump().size());
                        Economy restored(config);
                        restored.Load(save);
                        Check(save == restored.Save(), "Seasonal reload changed state");
                        restored.CollectStatistics(true);
                        e = std::move(restored);
                    }
                }
            }
            std::cout << "Completed seasonal seed " << seed << " (" << years << " years)\n"
                      << std::flush;
        }
        std::ofstream summary(dir / "summary.json");
        summary << json{{"seeds", seeds},
                        {"first_seed", first},
                        {"years", years},
                        {"scenario", selected},
                        {"maximum_stock_accounting_error", maxError},
                        {"maximum_sampled_save_bytes", maxSave}}
                       .dump(2)
                << '\n';
        for (auto *f : {&annual, &monthly, &roads, &summary}) {
            f->close();
            Check(bool(*f), "Cannot write seasonal output");
        }
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
