#include "core/Economy.h"
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <numeric>
#include <stdexcept>

using namespace marco;
namespace {
using Key = std::pair<std::string, std::string>;
constexpr double native = 3.105;
const std::vector<std::string> worlds{"unrest", "progressive_clear", "war_to_peace",
                                      "recurring_unrest"};
const std::vector<std::string> styles{"observer", "adventurer", "trader", "explorer"};
void Check(bool good, const char *why) {
    if (!good)
        throw std::runtime_error(why);
}
std::uint64_t Hash(std::uint64_t seed, std::uint64_t n) {
    auto x = seed + n * 0x9E3779B97F4A7C15ULL;
    x = (x ^ (x >> 30)) * 0xBF58476D1CE4E5B9ULL;
    x = (x ^ (x >> 27)) * 0x94D049BB133111EBULL;
    return x ^ (x >> 31);
}
std::uint64_t SiteSeed(const std::string &id) {
    std::uint64_t hash = 14695981039346656037ULL;
    for (unsigned char c : id) {
        hash ^= c;
        hash *= 1099511628211ULL;
    }
    return hash;
}
std::string Dealer(const std::string &good) {
    if (good == "grain" || good == "food" || good == "meat" || good == "drink")
        return "provisions";
    if (good == "ingredients" || good == "medicine")
        return "apothecary";
    if (good == "books" || good == "housewares")
        return "general";
    if (good == "silver" || good == "gold" || good == "gems" || good == "soul_gems")
        return "jeweler";
    return "smith";
}
std::string Shop(const std::string &market, const std::string &good) {
    return market + ':' + Dealer(good);
}
void Register(Economy &e, const std::string &market) {
    std::map<std::string, Merchant> shops;
    for (const auto &good : e.Definitions().commodities) {
        auto id = Shop(market, good.id);
        auto &shop = shops[id];
        shop.id = id;
        shop.market = market;
        shop.cash = 2000;
        int target = good.id == "weapons" || good.id == "armor"    ? 8
                     : good.id == "gems" || good.id == "soul_gems" ? 6
                                                                   : 20;
        for (int variant = 0; variant < 2; ++variant) {
            auto form = good.id + (variant ? ":b" : ":a");
            int quantity = variant ? target / 3 : target - target / 3;
            shop.items[form] = {form,     good.id, quantity,
                                quantity, 1,       good.referenceValue * (variant ? 1.5 : .75),
                                false};
        }
    }
    for (auto &[id, shop] : shops)
        e.RegisterMerchant(std::move(shop));
}
std::map<Key, int> Distances(const Config &c) {
    std::map<Key, int> d;
    for (const auto &a : c.markets)
        for (const auto &b : c.markets)
            d[{a.id, b.id}] = a.id == b.id ? 0 : 100000;
    for (const auto &r : c.routes)
        d[{r.from, r.to}] = std::min(d.at({r.from, r.to}), static_cast<int>(std::ceil(r.days)));
    for (const auto &k : c.markets)
        for (const auto &a : c.markets)
            for (const auto &b : c.markets)
                d[{a.id, b.id}] =
                    std::min(d.at({a.id, b.id}), d.at({a.id, k.id}) + d.at({k.id, b.id}));
    for (const auto &[key, days] : d)
        Check(days < 100000, "Disconnected player travel graph");
    return d;
}
SiteControl Initial(const SecuritySite &s) {
    if (s.kind == SiteKind::Crypt || s.kind == SiteKind::Unknown)
        return SiteControl::Unknown;
    if (s.kind == SiteKind::Fort && s.initialOccupants == SiteKind::Unknown)
        return SiteControl::Unknown;
    return SiteControl::Hostile;
}
void Conditions(Economy &e, const std::string &world, int day, unsigned seed, std::ostream &events,
                const std::string &prefix) {
    double war =
        (world == "war_to_peace" && day >= 365) || (world == "progressive_clear" && day >= 730)
            ? 0
            : .5;
    for (const auto &m : e.Definitions().markets)
        e.SetConditions(m.id, war, war * (m.id == "eastmarch" || m.id == "haafingar" ? .3 : .1));
    const auto &sites = e.Definitions().security.sites;
    for (std::size_t i = 0; i < sites.size(); ++i) {
        const auto &s = sites[i];
        auto control = Initial(s);
        if (s.kind == SiteKind::Crypt || s.kind == SiteKind::Unknown)
            continue;
        // Schedules are test inputs, never injected into the actual game.
        int clearDay = 45 + static_cast<int>(Hash(seed, SiteSeed(s.id)) % 650);
        if (world == "progressive_clear" && day >= clearDay)
            control = s.kind == SiteKind::Fort && day >= clearDay + 30 ? SiteControl::Imperial
                                                                       : SiteControl::Cleared;
        if (world == "war_to_peace" && day >= 365)
            control = s.kind == SiteKind::Fort ? SiteControl::Imperial : SiteControl::Cleared;
        if (world == "recurring_unrest") {
            auto phase = (day + Hash(seed, SiteSeed(s.id)) % 180) % 180;
            if (s.kind == SiteKind::Fort) {
                if (phase < 60)
                    control = Initial(s);
                else if (phase < 115)
                    control = SiteControl::Imperial;
                else if (phase < 125)
                    control = SiteControl::Contested;
                else
                    control = SiteControl::Stormcloak;
            } else
                control = phase < 120 ? SiteControl::Hostile : SiteControl::Cleared;
        }
        if (e.ObserveSite(s.id, control, day))
            events << prefix << ',' << day << ',' << s.id << ',' << ControlName(control) << '\n';
    }
}
struct Player {
    std::string market{"whiterun"}, destination, good, form;
    int arrives{}, cargo{}, trips{}, bought{}, sold{}, looted{}, cashLimited{}, tour{};
    Money wallet{1000}, paid{}, received{};
};
Money Price(const Economy &e, const std::string &market, const std::string &form,
            const std::string &good, int count, bool buy) {
    const auto &item = e.Get().merchants.at(Shop(market, good)).items.at(form);
    return e
        .Price(market, good,
               item.value * e.Definitions().BarterMultiplier(buy ? native : 1 / native, buy), count,
               buy)
        .total;
}
void Transfer(Economy &e, Player &p, int count, bool buy, int day, const std::string &prefix,
              std::ostream &trades) {
    auto id = Shop(p.market, p.good);
    const auto &m = e.Get().merchants.at(id);
    auto gold = Price(e, p.market, p.form, p.good, count, buy);
    auto before = m.cash + p.wallet;
    auto stock = m.items.at(p.form).quantity + p.cargo;
    e.Trade(id, p.form, count, buy, gold);
    p.wallet += buy ? -gold : gold;
    p.cargo += buy ? count : -count;
    if (buy) {
        p.paid += gold;
        p.bought += count;
    } else {
        p.received += gold;
        p.sold += count;
    }
    Check(p.wallet >= 0 && p.cargo >= 0 && m.cash + p.wallet == before,
          "Player gold accounting failure");
    Check(m.items.at(p.form).quantity + p.cargo == stock, "Player stock accounting failure");
    trades << prefix << ',' << day << ',' << p.market << ',' << p.good << ','
           << (buy ? "buy" : "sell") << ',' << count << ',' << gold << ',' << p.wallet << '\n';
}
void Act(Economy &e, Player &p, const std::string &style, int day, int end, unsigned seed,
         const std::map<Key, int> &travel, const std::string &prefix, std::ostream &trades) {
    if (style == "observer" || style == "explorer")
        return;
    if (!p.destination.empty()) {
        if (day < p.arrives)
            return;
        p.market = p.destination;
        p.destination.clear();
    }
    auto move = [&](const std::string &to) {
        Check(to != p.market, "Zero-time journey");
        p.arrives = day + travel.at({p.market, to});
        p.destination = to;
    };
    if (p.cargo) {
        int count = p.cargo;
        auto cash = e.Get().merchants.at(Shop(p.market, p.good)).cash;
        while (count && Price(e, p.market, p.form, p.good, count, false) > cash)
            --count;
        if (count < p.cargo)
            ++p.cashLimited;
        if (count)
            Transfer(e, p, count, false, day, prefix, trades);
        if (p.cargo)
            return;
        ++p.trips;
        return; // The next decision takes place on a later game day.
    }
    if (style == "adventurer") {
        if (day % 7)
            return;
        const std::vector<std::string> loot{"weapons",    "armor", "ingredients",
                                            "housewares", "gems",  "books"};
        p.good = loot[Hash(seed, day) % loot.size()];
        p.form = p.good + ":b";
        p.cargo = p.good == "ingredients" || p.good == "housewares" ? 3 : 1;
        p.looted += p.cargo;
        const auto &markets = e.Definitions().markets;
        auto to = markets[(++p.tour) % markets.size()].id;
        if (to != p.market)
            move(to);
        return;
    }
    double best = 0;
    std::string chosenGood, chosenForm, chosenMarket;
    int chosenCount = 0;
    for (const auto &[id, shop] : e.Get().merchants) {
        if (shop.market != p.market)
            continue;
        for (const auto &[form, item] : shop.items) {
            double weight = item.commodity == "armor" ? 20 : item.commodity == "weapons" ? 10 : 2;
            int count = std::min({20, item.quantity, static_cast<int>(120 / weight)});
            while (count && Price(e, p.market, form, item.commodity, count, true) > p.wallet)
                --count;
            if (!count)
                continue;
            auto paid = Price(e, p.market, form, item.commodity, count, true);
            for (const auto &target : e.Definitions().markets) {
                if (target.id == p.market)
                    continue;
                int days = travel.at({p.market, target.id});
                if (day + days > end)
                    continue;
                auto received = Price(e, target.id, form, item.commodity, count, false);
                if (received > e.Get().merchants.at(Shop(target.id, item.commodity)).cash)
                    continue;
                // Perfect knowledge of today's offers is an intentional upper-bound
                // player strategy; it does not know prices on the day of arrival.
                double score = static_cast<double>(received - paid) / (days + 1);
                if (score > best) {
                    best = score;
                    chosenGood = item.commodity;
                    chosenForm = form;
                    chosenMarket = target.id;
                    chosenCount = count;
                }
            }
        }
    }
    if (chosenCount) {
        p.good = chosenGood;
        p.form = chosenForm;
        Transfer(e, p, chosenCount, true, day, prefix, trades);
        move(chosenMarket);
    } else if (day + 7 <= end) {
        auto to = p.market == "whiterun" ? "rift" : "whiterun";
        move(to);
    }
}
struct GoodMetrics {
    double samples{}, price{}, stock{}, floor{}, ceiling{}, wanted{}, supplied{}, shopStock{},
        offers{}, empty{};
};
struct Metrics {
    double days{}, marketSamples{}, prices{}, floor{}, ceiling{}, wanted{}, supplied{}, arrived{},
        departed{};
    double offers{}, empty{}, merchantDays{}, lowCash{}, merchantGold{}, wallet{};
    std::map<Key, GoodMetrics> goods;
    void Add(const Economy &e, const Player &player) {
        ++days;
        wallet += player.wallet;
        const auto &stats = *e.Statistics();
        arrived += stats.arrived;
        departed += stats.departed;
        for (const auto &sample : stats.demand) {
            auto &g = goods[{sample.market, sample.commodity}];
            g.wanted += sample.wanted;
            g.supplied += sample.supplied;
            wanted += sample.wanted;
            supplied += sample.supplied;
        }
        for (const auto &[market, m] : e.Get().markets)
            for (const auto &[good, s] : m.goods) {
                auto &g = goods[{market, good}];
                ++g.samples;
                g.price += s.price;
                g.stock += s.quantity;
                bool lo = s.price <= e.Definitions().minMultiplier + 1e-8;
                bool hi = s.price >= e.Definitions().maxMultiplier - 1e-8;
                g.floor += lo;
                g.ceiling += hi;
                ++marketSamples;
                prices += s.price;
                floor += lo;
                ceiling += hi;
            }
        for (const auto &[id, shop] : e.Get().merchants) {
            ++merchantDays;
            merchantGold += shop.cash;
            lowCash += shop.cash < 100;
            for (const auto &[form, item] : shop.items) {
                auto &g = goods[{shop.market, item.commodity}];
                g.shopStock += item.quantity;
                ++g.offers;
                g.empty += item.quantity == 0;
                ++offers;
                empty += item.quantity == 0;
            }
        }
    }
    json Values() const {
        return {{"days", days},
                {"price_index", prices / marketSamples},
                {"price_floor_fraction", floor / marketSamples},
                {"price_ceiling_fraction", ceiling / marketSamples},
                {"unmet_demand_fraction", wanted ? 1 - supplied / wanted : 0},
                {"empty_offer_fraction", offers ? empty / offers : 0},
                {"low_cash_fraction", merchantDays ? lowCash / merchantDays : 0},
                {"mean_merchant_gold", merchantDays ? merchantGold / merchantDays : 0},
                {"mean_player_gold", wallet / days},
                {"delivered_per_day", arrived / days},
                {"dispatched_per_day", departed / days}};
    }
};
std::map<std::string, double> InitialStock(const Economy &e) {
    std::map<std::string, double> out;
    for (const auto &[id, m] : e.Get().markets)
        for (const auto &[good, s] : m.goods)
            out[good] += s.quantity;
    return out;
}
double Accounting(const Economy &e, const std::map<std::string, double> &initial) {
    auto residual = initial;
    for (const auto &[id, m] : e.Get().markets)
        for (const auto &[good, s] : m.goods)
            residual[good] +=
                s.produced - s.consumed + s.imported - s.exported - s.lost - s.quantity;
    for (const auto &[id, m] : e.Get().merchants)
        for (const auto &[form, i] : m.items)
            residual[i.commodity] -= (i.quantity + i.externalQuantity) * i.units;
    double worst = 0;
    for (const auto &[id, n] : residual)
        worst = std::max(worst, std::abs(n));
    Check(worst < 1e-5, "Campaign stock accounting failed");
    return worst;
}
void Row(std::ostream &out, const std::string &prefix, int period, const Metrics &m) {
    auto j = m.Values();
    out << prefix << ',' << period;
    for (const char *key :
         {"price_index", "price_floor_fraction", "price_ceiling_fraction", "unmet_demand_fraction",
          "empty_offer_fraction", "low_cash_fraction", "mean_merchant_gold", "mean_player_gold",
          "delivered_per_day", "dispatched_per_day"})
        out << ',' << j.at(key).get<double>();
    out << '\n';
}
} // namespace
int main(int argc, char **argv) {
    try {
        Check(argc >= 4 && argc <= 7, "marco_campaign_sim economy.json security.json output "
                                      "[seeds=16] [days=1825] [first-seed=1]");
        std::ifstream ef(argv[1]), sf(argv[2]);
        auto definitions = json::parse(ef);
        definitions["security"] = json::parse(sf);
        auto config = Config::Parse(definitions);
        unsigned seeds = argc > 4 ? std::stoul(argv[4]) : 16;
        int end = argc > 5 ? std::stoi(argv[5]) : 1825;
        unsigned first = argc > 6 ? std::stoul(argv[6]) : 1;
        Check(seeds && seeds <= 256 && first && first <= 10000 && end >= 60 && end <= 3650,
              "Invalid campaign dimensions");
        std::filesystem::path out(argv[3]);
        Check(!std::filesystem::exists(out) || std::filesystem::is_empty(out),
              "Use an empty output directory");
        std::filesystem::create_directories(out);
        std::ofstream monthly(out / "monthly.csv"), yearly(out / "yearly.csv"),
            goods(out / "goods.csv"), events(out / "events.csv"), trades(out / "trades.csv");
        const char *header =
            "seed,world,player,period,price_index,price_floor_fraction,price_ceiling_fraction,"
            "unmet_demand_fraction,empty_offer_fraction,low_cash_fraction,mean_merchant_gold,mean_"
            "player_gold,delivered_per_day,dispatched_per_day\n";
        monthly << header;
        yearly << header;
        goods << "seed,world,player,year,market,good,mean_price,mean_warehouse,mean_shop_stock_per_"
                 "offer,empty_fraction,ceiling_fraction,unmet_demand_fraction\n";
        events << "seed,world,player,day,site,control\n";
        trades << "seed,world,player,day,market,good,action,quantity,gold,wallet\n";
        for (auto *f : {&monthly, &yearly, &goods, &events, &trades})
            *f << std::setprecision(12);
        json result{
            {"seeds", seeds},
            {"first_seed", first},
            {"end_day", end},
            {"warmup_days", 30},
            {"merchant_starting_gold", 2000},
            {"player_starting_gold", 1000},
            {"assumptions",
             "Synthetic multi-category shops, two variants per good; no travel expenses; trader "
             "sees current offers but not future prices; campaign events are hypothetical inputs"},
            {"runs", json::array()}};
        auto travel = Distances(config);
        double worstError = 0;
        std::size_t maxSave = 0, maxShipments = 0;
        for (unsigned seed = first; seed < first + seeds; ++seed) {
            for (const auto &world : worlds)
                for (const auto &style : styles) {
                    std::string prefix = std::to_string(seed) + ',' + world + ',' + style;
                    Economy e(config);
                    e.NewGame(0, seed);
                    e.CollectStatistics(true);
                    auto initial = InitialStock(e);
                    std::size_t registered = style == "explorer" ? 1 : config.markets.size();
                    for (std::size_t i = 0; i < registered; ++i)
                        Register(e, config.markets[i].id);
                    Player player;
                    Metrics month, year;
                    json run{{"seed", seed},
                             {"world", world},
                             {"player", style},
                             {"years", json::array()}};
                    for (int day = 1; day <= end; ++day) {
                        Conditions(e, world, day - 1, seed, events, prefix);
                        if (style == "explorer" && day % 30 == 0 &&
                            registered < config.markets.size())
                            Register(e, config.markets[registered++].id);
                        e.Advance(day);
                        if (day <= 30)
                            continue;
                        Act(e, player, style, day, end, seed, travel, prefix, trades);
                        month.Add(e, player);
                        year.Add(e, player);
                        maxShipments = std::max(maxShipments, e.Get().shipments.size());
                        if (day % 30 == 0 || day == end) {
                            Row(monthly, prefix, (day - 31) / 30 + 1, month);
                            month = {};
                            worstError = std::max(worstError, Accounting(e, initial));
                            e.Validate();
                        }
                        if (day == 47 || day % 365 == 0) {
                            auto saved = e.Save();
                            maxSave = std::max(maxSave, saved.dump().size());
                            Economy restored(config);
                            restored.Load(saved);
                            Check(restored.Save() == saved, "Campaign reload changed saved state");
                            restored.CollectStatistics(true);
                            e = std::move(restored);
                        }
                        if ((day - 30) % 365 == 0 || day == end) {
                            int n = (day - 31) / 365 + 1;
                            Row(yearly, prefix, n, year);
                            run["years"].push_back(year.Values());
                            for (const auto &[key, g] : year.goods) {
                                goods << prefix << ',' << n << ',' << key.first << ',' << key.second
                                      << ',' << g.price / g.samples << ',' << g.stock / g.samples
                                      << ',' << (g.offers ? g.shopStock / g.offers : 0) << ','
                                      << (g.offers ? g.empty / g.offers : 0) << ','
                                      << g.ceiling / g.samples << ','
                                      << (g.wanted ? 1 - g.supplied / g.wanted : 0) << '\n';
                            }
                            year = {};
                        }
                    }
                    Check(player.wallet == 1000 + player.received - player.paid,
                          "Campaign wallet accounting failed");
                    Check(player.bought + player.looted == player.sold + player.cargo,
                          "Campaign cargo accounting failed");
                    run["player_totals"] = {{"gold", player.wallet},
                                            {"paid", player.paid},
                                            {"received", player.received},
                                            {"bought", player.bought},
                                            {"looted", player.looted},
                                            {"sold", player.sold},
                                            {"unsold", player.cargo},
                                            {"completed_sales", player.trips},
                                            {"cash_limited_days", player.cashLimited}};
                    run["final_merchants"] = e.Get().merchants.size();
                    run["final_save_bytes"] = e.Save().dump().size();
                    result["runs"].push_back(std::move(run));
                }
            std::cout << "Completed campaign seed " << seed << " (16 scenarios, " << end
                      << " days each)\n"
                      << std::flush;
        }
        result["maximum_stock_accounting_error"] = worstError;
        result["maximum_save_bytes"] = maxSave;
        result["maximum_shipments"] = maxShipments;
        std::ofstream summary(out / "summary.json");
        summary << result.dump(2) << '\n';
        summary.close();
        for (auto *f : {&monthly, &yearly, &goods, &events, &trades}) {
            f->close();
            Check(bool(*f), "Cannot write campaign output");
        }
        Check(bool(summary), "Cannot write campaign summary");
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
