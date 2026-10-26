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
constexpr double nativeBarter = 3.105;
struct Journey {
    std::string good, from, to;
    int amount{}, days{};
    std::string ID() const {
        return good + ":" + from + "-" + to;
    }
};
struct Trader {
    Journey journey;
    Money wallet{10000}, paid{}, received{};
    int cargo{}, bought{}, sold{}, trips{}, skipped{}, partialSales{};
    int nextBuy{31}, arrival{};
};
struct Sample {
    double count{}, stock{}, target{}, empty{}, buy{}, sell{}, cash{};
};
void Require(bool condition, const char *why) {
    if (!condition)
        throw std::runtime_error(why);
}
Money Price(const Economy &e, const std::string &market, const std::string &good, int count,
            bool buying) {
    return e
        .Price(
            market, good,
            e.Definitions().Good(good).referenceValue *
                e.Definitions().BarterMultiplier(buying ? nativeBarter : 1 / nativeBarter, buying),
            count, buying)
        .total;
}
std::string ShopID(const std::string &market, const std::string &good) {
    return market + ":" + good;
}
void Shops(Economy &e, bool fixture) {
    // One independently funded representative dealer per tracked commodity.
    // These are synthetic inventories, not reconstructed Skyrim NPC chests.
    const std::map<std::string, int> full{{"food", 20},   {"drink", 20},  {"iron", 20},
                                          {"weapons", 8}, {"silver", 12}, {"housewares", 12}};
    const std::map<std::string, int> small{{"iron", 30}, {"food", 20}};
    for (const auto &m : e.Definitions().markets)
        for (const auto &[good, quantity] : fixture ? small : full) {
            Merchant shop{ShopID(m.id, good), m.id, 2000, {}, 0, 0};
            shop.items[good] = {good,     good, quantity,
                                quantity, 1,    e.Definitions().Good(good).referenceValue,
                                false};
            e.RegisterMerchant(shop);
        }
}
Config Fixture(const Config &full) {
    Config c;
    c.barterExponent = full.barterExponent;
    c.minimumBarterMargin = full.minimumBarterMargin;
    c.commodities = {{"iron", "Iron", 12, .8}, {"food", "Food", 8, .6}};
    c.markets = {{"source",
                  "Source",
                  "",
                  1,
                  0,
                  {{"iron", 38}, {"food", 25}},
                  {{"iron", 10}, {"food", 10}},
                  {}},
                 {"destination",
                  "Destination",
                  "",
                  1,
                  0,
                  {{"iron", 0}, {"food", 4}},
                  {{"iron", 20}, {"food", 10}},
                  {}}};
    c.routes = {{"road", "source", "destination", 3, 32, .04, .03}};
    c.security.enabled = true;
    c.security.sites = {{"camp",
                         "Roadside camp",
                         "destination",
                         SiteKind::Bandit,
                         SiteKind::Unknown,
                         1.8,
                         0,
                         {{"road", 1}},
                         {}}};
    return c;
}
void Change(Economy &e, const std::string &scenario) {
    if (scenario == "valtheim_cleared")
        e.ObserveSite("Skyrim.esm|0192C1", SiteControl::Cleared, 30);
    if (scenario == "greymoor_imperial")
        e.ObserveSite("Skyrim.esm|01917D", SiteControl::Imperial, 30);
    if (scenario == "all_raiders_cleared")
        for (const auto &site : e.Definitions().security.sites)
            if (site.kind != SiteKind::Unknown && site.kind != SiteKind::Crypt)
                e.ObserveSite(site.id, SiteControl::Cleared, 30);
}
void Trade(Economy &e, Trader &t, int day, std::ostream &csv, const std::string &prefix) {
    const auto &j = t.journey;
    auto transfer = [&](int count, Money gold, bool buying) {
        const auto &shop = e.Get().merchants.at(ShopID(buying ? j.from : j.to, j.good));
        auto cash = shop.cash, wallet = t.wallet;
        auto quantity = shop.items.at(j.good).quantity;
        e.Trade(shop.id, j.good, count, buying, gold);
        t.wallet += buying ? -gold : gold;
        t.cargo += buying ? count : -count;
        Require(t.wallet >= 0 && t.cargo >= 0, "Invalid trader balance");
        Require(shop.cash + t.wallet == cash + wallet, "Trade created or lost gold");
        Require(shop.items.at(j.good).quantity == quantity + (buying ? -count : count),
                "Incorrect stock transfer");
        csv << prefix << ',' << day << ',' << j.ID() << ',' << (buying ? "buy" : "sell") << ','
            << count << ',' << gold << ',' << shop.cash << ',' << t.wallet << '\n';
    };
    if (t.cargo > 0 && day >= t.arrival) {
        auto cash = e.Get().merchants.at(ShopID(j.to, j.good)).cash;
        int count = t.cargo;
        while (count && Price(e, j.to, j.good, count, false) > cash)
            --count;
        if (count < t.cargo)
            ++t.partialSales;
        if (count) {
            Money gold = Price(e, j.to, j.good, count, false);
            transfer(count, gold, false);
            t.sold += count;
            t.received += gold;
            if (t.cargo == 0) {
                ++t.trips;
                // Allow the return journey before the next purchase.
                t.nextBuy = std::max(t.nextBuy, day + j.days);
            }
        }
    }
    if (t.cargo == 0 && day >= t.nextBuy && day + j.days <= 90) {
        int count = std::min(
            j.amount, e.Get().merchants.at(ShopID(j.from, j.good)).items.at(j.good).quantity);
        while (count && Price(e, j.from, j.good, count, true) > t.wallet)
            --count;
        t.nextBuy = day + 14;
        if (!count) {
            ++t.skipped;
            return;
        }
        Money gold = Price(e, j.from, j.good, count, true);
        transfer(count, gold, true);
        t.paid += gold;
        t.bought += count;
        t.arrival = day + j.days;
    }
}
std::map<std::string, double> Initial(const Economy &e) {
    std::map<std::string, double> out;
    for (const auto &[id, m] : e.Get().markets)
        for (const auto &[good, stock] : m.goods)
            out[good] += stock.quantity;
    return out;
}
double Conservation(const Economy &e, const std::map<std::string, double> &initial) {
    auto residual = initial;
    for (const auto &[id, m] : e.Get().markets)
        for (const auto &[good, s] : m.goods)
            residual[good] +=
                s.produced - s.consumed + s.imported - s.exported - s.lost - s.quantity;
    for (const auto &[id, m] : e.Get().merchants)
        for (const auto &[form, item] : m.items)
            residual[item.commodity] -= (item.quantity + item.externalQuantity) * item.units;
    double error = 0;
    for (const auto &[good, value] : residual)
        error = std::max(error, std::abs(value));
    Require(error < 1e-6, "Stock accounting failed");
    return error;
}
} // namespace
int main(int argc, char **argv) {
    try {
        if (argc < 4 || argc > 5) {
            std::cerr
                << "marco_merchant_sim economy.json security.json output-directory [seeds=32]\n";
            return 2;
        }
        std::ifstream ef(argv[1]), sf(argv[2]);
        auto definitions = json::parse(ef);
        definitions["security"] = json::parse(sf);
        auto full = Config::Parse(definitions);
        unsigned seeds = argc == 5 ? static_cast<unsigned>(std::stoul(argv[4])) : 32;
        Require(seeds > 0 && seeds <= 1000, "Use 1..1000 seeds");
        std::filesystem::path out(argv[3]);
        Require(!std::filesystem::exists(out) || std::filesystem::is_empty(out),
                "Use an empty output directory");
        std::filesystem::create_directories(out);
        std::ofstream daily(out / "shops.csv"), trades(out / "trades.csv");
        daily << std::setprecision(12);
        daily << "model,seed,scenario,day,market,good,stock,target,buy_one,sell_one,cash\n";
        trades << "model,seed,scenario,day,journey,action,quantity,gold,merchant_cash,wallet\n";
        json summary{{"seeds", seeds},
                     {"warmup_days", 30},
                     {"end_day", 90},
                     {"native_buy_multiplier", nativeBarter},
                     {"merchant_starting_cash", 2000},
                     {"stock_units_per_item", 1},
                     {"expenses", "None; profits are before travel expenses"},
                     {"inventories", "Synthetic dealers; values use commodity reference values"}};
        double error = 0;
        for (bool fixture : {false, true}) {
            const std::string model = fixture ? "import_fixture" : "shipped";
            auto config = fixture ? Fixture(full) : full;
            std::vector<std::string> scenarios{"unchanged", "all_raiders_cleared"};
            if (!fixture) {
                scenarios.push_back("valtheim_cleared");
                scenarios.push_back("greymoor_imperial");
            }
            std::vector<Journey> journeys =
                fixture ? std::vector<Journey>{{"iron", "source", "destination", 5, 3}}
                        : std::vector<Journey>{{"drink", "rift", "pale", 10, 5},
                                               {"weapons", "whiterun", "winterhold", 5, 4},
                                               {"silver", "reach", "haafingar", 10, 3},
                                               {"housewares", "rift", "winterhold", 10, 6}};
            std::map<std::string, std::map<std::string, Sample>> samples;
            for (unsigned seed = 1; seed <= seeds; ++seed) {
                Economy warm(config);
                warm.NewGame(0, seed);
                const auto initial = Initial(warm);
                Shops(warm, fixture);
                for (const auto &site : config.security.sites)
                    if (site.kind != SiteKind::Unknown && site.kind != SiteKind::Crypt)
                        warm.ObserveSite(site.id, SiteControl::Hostile, 0);
                if (!fixture)
                    for (const auto &m : config.markets)
                        warm.SetConditions(m.id, .5,
                                           m.id == "eastmarch" || m.id == "haafingar" ? .15 : .05);
                warm.Advance(30, 30);
                auto common = warm.Save();
                for (const auto &scenario : scenarios) {
                    Economy e(config);
                    e.Load(common);
                    Change(e, scenario);
                    std::vector<Trader> players;
                    for (const auto &journey : journeys)
                        players.push_back({journey});
                    std::string prefix = model + ',' + std::to_string(seed) + ',' + scenario;
                    for (int day = 31; day <= 90; ++day) {
                        e.Advance(day);
                        for (auto &player : players)
                            Trade(e, player, day, trades, prefix);
                        // Day 47 is during several journeys; reload must preserve all
                        // merchant funds, restocking, shipment and security state.
                        if (day == 47) {
                            auto saved = e.Save();
                            Economy restored(config);
                            restored.Load(saved);
                            Require(restored.Save() == saved, "Reload changed economy");
                            e = std::move(restored);
                        }
                        for (const auto &[id, shop] : e.Get().merchants)
                            for (const auto &[good, item] : shop.items) {
                                auto buy = Price(e, shop.market, good, 1, true);
                                auto sell = Price(e, shop.market, good, 1, false);
                                daily << prefix << ',' << day << ',' << shop.market << ',' << good
                                      << ',' << item.quantity << ',' << item.target << ',' << buy
                                      << ',' << sell << ',' << shop.cash << '\n';
                                auto &s = samples[scenario][id];
                                ++s.count;
                                s.stock += item.quantity;
                                s.target += item.target;
                                s.empty += item.quantity == 0;
                                s.buy += buy;
                                s.sell += sell;
                                s.cash += shop.cash;
                            }
                        error = std::max(error, Conservation(e, initial));
                    }
                    e.Validate();
                    for (const auto &t : players) {
                        Require(t.bought == t.sold + t.cargo, "Trader inventory accounting failed");
                        Require(t.wallet == 10000 + t.received - t.paid,
                                "Trader gold accounting failed");
                        auto &j = summary["models"][model][scenario]["trades"][t.journey.ID()];
                        if (j.is_null())
                            j = {{"paid", 0},
                                 {"received", 0},
                                 {"bought", 0},
                                 {"sold", 0},
                                 {"unsold", 0},
                                 {"completed_trips", 0},
                                 {"skipped_departures", 0},
                                 {"cash_limited_sale_days", 0},
                                 {"profitable_seeds", 0},
                                 {"minimum_net", t.received - t.paid},
                                 {"maximum_net", t.received - t.paid}};
                        j["paid"] = j["paid"].get<Money>() + t.paid;
                        j["received"] = j["received"].get<Money>() + t.received;
                        for (auto [key, n] : std::initializer_list<std::pair<const char *, int>>{
                                 {"bought", t.bought},
                                 {"sold", t.sold},
                                 {"unsold", t.cargo},
                                 {"completed_trips", t.trips},
                                 {"skipped_departures", t.skipped},
                                 {"cash_limited_sale_days", t.partialSales},
                                 {"profitable_seeds", t.received > t.paid}})
                            j[key] = j[key].get<int>() + n;
                        j["minimum_net"] =
                            std::min(j["minimum_net"].get<Money>(), t.received - t.paid);
                        j["maximum_net"] =
                            std::max(j["maximum_net"].get<Money>(), t.received - t.paid);
                    }
                }
            }
            for (const auto &[scenario, shops] : samples)
                for (const auto &[id, s] : shops)
                    summary["models"][model][scenario]["shops"][id] = {
                        {"mean_stock", s.stock / s.count},
                        {"mean_target", s.target / s.count},
                        {"empty_day_fraction", s.empty / s.count},
                        {"mean_buy_one", s.buy / s.count},
                        {"mean_sell_one", s.sell / s.count},
                        {"mean_cash", s.cash / s.count}};
        }
        summary["maximum_stock_accounting_error"] = error;
        std::ofstream result(out / "summary.json");
        result << summary.dump(2) << '\n';
        result.close();
        daily.close();
        trades.close();
        Require(result && daily && trades, "Cannot write simulation results");
        std::cout << seeds
                  << " paired seeds; 60 dealers, four full-economy scenarios and two "
                     "import-dependent scenarios; wrote "
                  << out.string() << '\n';
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
