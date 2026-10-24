#include "core/Economy.h"
#include <algorithm>
#include <cmath>
#include <fstream>
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
template <class F> void Rejects(F f) {
    bool rejected = false;
    try {
        f();
    } catch (const std::exception &) {
        rejected = true;
    }
    CHECK(rejected);
}
struct Route {
    const char *good, *source, *destination;
    int quantity, days;
};
constexpr Route routes[]{{"drink", "rift", "pale", 10, 5},
                         {"weapons", "whiterun", "winterhold", 5, 4},
                         {"silver", "reach", "haafingar", 10, 3},
                         {"housewares", "rift", "winterhold", 10, 6}};

Merchant Shop(const char *id, const char *market, const char *good, int quantity, double value) {
    Merchant m{id, market, 2000, {}, 0, 0};
    m.items["cargo"] = {"cargo", good, quantity, quantity, 1, value, false};
    return m;
}
Money QuoteFor(const Economy &e, const Route &r, bool buy, double native, int quantity = -1) {
    return e
        .Price(buy ? r.source : r.destination, r.good,
               e.Definitions().Good(r.good).referenceValue *
                   e.Definitions().BarterMultiplier(native, buy),
               quantity < 0 ? r.quantity : quantity, buy)
        .total;
}
void TestShipment(const Config &config, const Route &r, int age, std::uint64_t seed, bool war) {
    Economy e(config);
    e.NewGame(0, seed);
    if (war)
        for (auto &m : config.markets)
            e.SetConditions(m.id, .5, m.id == "eastmarch" || m.id == "haafingar" ? .15 : .05);
    e.Advance(age, 365);
    double value = config.Good(r.good).referenceValue;
    e.RegisterMerchant(Shop("source", r.source, r.good, r.quantity * 2, value));
    e.RegisterMerchant(Shop("destination", r.destination, r.good, 5, value));
    auto starting = e.Save();
    constexpr double novice = 3.105;
    Money paid = QuoteFor(e, r, true, novice), wallet = 1000;
    CHECK(paid <= wallet);
    CHECK(QuoteFor(e, r, true, 2.0) <= paid); // Speech still improves the offer.
    CHECK(e.Save() == starting);              // Merely looking cannot move a market.
    e.Trade("source", "cargo", r.quantity, true, paid);
    wallet -= paid;
    CHECK(e.Get().merchants.at("source").cash == 2000 + paid);
    CHECK(QuoteFor(e, r, true, novice) >= paid);
    // The same goods return less in their source hold, even after our purchase
    // has raised its price. A destination shortage is what pays for the trip.
    auto local = e.Price(r.source, r.good, value * config.BarterMultiplier(1 / novice, false),
                         r.quantity, false);
    CHECK(local.total <= paid);
    e.Advance(age + r.days);
    // Reload during the journey: no regenerated regional prices or free stocks.
    auto travelling = e.Save();
    Economy reloaded(config);
    reloaded.Load(travelling);
    CHECK(reloaded.Save() == travelling);
    Money received = QuoteFor(reloaded, r, false, 1 / novice);
    CHECK(received == QuoteFor(e, r, false, 1 / novice));
    CHECK(QuoteFor(e, r, false, .5) >= received);
    CHECK(received <= reloaded.Get().merchants.at("destination").cash);
    CHECK(received >= paid * 2);     // Selected long routes can double the cargo outlay.
    CHECK(received - paid - 40 > 0); // Also clear an illustrative 40-gold travel expense.
    auto destinationBefore = reloaded.Get().merchants.at("destination");
    auto priceBefore = reloaded.Multiplier(r.destination, r.good);
    reloaded.Trade("destination", "cargo", r.quantity, false, received);
    wallet += received - 40;
    CHECK(wallet > 1000);
    auto &destination = reloaded.Get().merchants.at("destination");
    CHECK(destination.cash == destinationBefore.cash - received);
    CHECK(destination.items.at("cargo").quantity ==
          destinationBefore.items.at("cargo").quantity + r.quantity);
    CHECK(reloaded.Multiplier(r.destination, r.good) < priceBefore);
    CHECK(QuoteFor(reloaded, r, false, 1 / novice) <= received);
    // A higher quote cannot manufacture a merchant's payment funds.
    reloaded.Edit().merchants.at("destination").cash = 0;
    auto emptyPurse = reloaded.Save();
    Rejects([&] { reloaded.Trade("destination", "cargo", r.quantity, false, received); });
    CHECK(reloaded.Save() == emptyPurse);
    if (age == 30 && seed == 42 && war)
        std::cout << r.good << ": " << r.source << " -> " << r.destination << ", " << r.quantity
                  << " items, " << r.days << " days, paid=" << paid << ", received=" << received
                  << ", net after 40 travel=" << received - paid - 40 << '\n';
}
int main(int argc, char **argv) {
    try {
        CHECK(argc == 2);
        std::ifstream f(argv[1]);
        auto definitions = json::parse(f);
        auto config = Config::Parse(definitions);
        CHECK(config.BarterMultiplier(3.105, true) < 1.3);
        CHECK(config.BarterMultiplier(1 / 3.105, false) > .75);
        CHECK(config.BarterMultiplier(.1, true) >= 1.05);
        CHECK(config.BarterMultiplier(10, false) <= .95);
        for (double bad : {0.0, -1.0, std::numeric_limits<double>::infinity(), std::nan("")})
            Rejects([&] { config.BarterMultiplier(bad, true); });
        for (const char *key : {"barterExponent", "minimumBarterMargin"}) {
            auto bad = definitions;
            bad["policy"][key] = -1;
            Rejects([&] { Config::Parse(bad); });
            bad["policy"][key] = 2;
            Rejects([&] { Config::Parse(bad); });
        }
        auto bad = definitions;
        bad["markets"][0]["priceFactors"]["weapons"] = 0;
        Rejects([&] { Config::Parse(bad); });
        bad = definitions;
        bad["markets"][0]["priceFactors"]["missing-good"] = 1;
        Rejects([&] { Config::Parse(bad); });
        // Older/custom configurations keep their original pricing policy.
        auto legacyDefinitions = definitions;
        legacyDefinitions["policy"].erase("barterExponent");
        legacyDefinitions["policy"].erase("minimumBarterMargin");
        for (auto &market : legacyDefinitions["markets"])
            market.erase("priceFactors");
        auto oldConfig = Config::Parse(legacyDefinitions);
        CHECK(oldConfig.BarterMultiplier(3.105, true) == 3.105);
        CHECK(oldConfig.BarterMultiplier(.322, false) == .322);
        Economy old(oldConfig), updated(config);
        old.Advance(7);
        auto preserved = old.Save();
        updated.Load(preserved);
        CHECK(updated.Save() == preserved); // Updating the policy never resets a save.
        updated.Advance(8);
        for (auto &[market, data] : updated.Get().markets)
            for (auto &[good, stock] : data.goods)
                CHECK(std::abs(stock.price - old.Get().markets.at(market).goods.at(good).price) <=
                      config.dailyAdjustment + 1e-9);
        // Reclassifying an already observed bottle from Food to Drink moves
        // accounting, without changing the actual bottle count or merchant cash.
        auto wine = Shop("wine", "rift", "food", 12, 20);
        updated.RegisterMerchant(wine);
        auto imports = updated.Get().markets.at("rift").goods.at("drink").imported;
        wine.items.at("cargo").commodity = "drink";
        updated.RegisterMerchant(wine);
        CHECK(updated.Get().merchants.at("wine").cash == wine.cash);
        CHECK(updated.Get().merchants.at("wine").items.at("cargo").quantity == 12);
        CHECK(updated.Get().markets.at("rift").goods.at("drink").imported == imports + 12);
        // Dear import goods must not bankrupt a merchant through ordinary NPC
        // turnover. The warehouse purchase and customer receipt share a scale.
        auto turnoverConfig = config;
        turnoverConfig.merchantTurnover = 1;
        Economy business(turnoverConfig);
        auto importer = Shop("importer", "winterhold", "weapons", 5, 60);
        business.RegisterMerchant(importer);
        business.Advance(1);
        CHECK(business.Get().merchants.at("importer").items.at("cargo").quantity == 5);
        CHECK(business.Get().merchants.at("importer").cash > importer.cash);
        business.NewGame(0, 42);
        importer.cash = 999999999;
        business.RegisterMerchant(importer);
        business.Advance(1);
        business.Validate();
        for (int age : {0, 30, 90})
            for (std::uint64_t seed : {7, 42})
                for (bool war : {false, true})
                    for (auto &route : routes)
                        TestShipment(config, route, age, seed, war);
        // Every market and commodity: extreme opposing merchant bonuses must
        // not turn a same-day local resale into a money printer, including the
        // cheap goods where whole-gold rounding dominates.
        for (auto &m : config.markets)
            for (auto &c : config.commodities)
                for (double value : {1.0, 5.0, 100.0}) {
                    Economy e(config);
                    auto a = Shop("a", m.id.c_str(), c.id.c_str(), 100, value);
                    auto b = Shop("b", m.id.c_str(), c.id.c_str(), 0, value);
                    a.cash = b.cash = 100000;
                    for (int n : {1, 10, 100}) {
                        e.NewGame(0, 42);
                        e.RegisterMerchant(a);
                        e.RegisterMerchant(b);
                        auto paid =
                            e.Price(m.id, c.id, value * config.BarterMultiplier(.1, true), n, true);
                        e.Trade("a", "cargo", n, true, paid.total);
                        auto sold = e.Price(m.id, c.id, value * config.BarterMultiplier(10, false),
                                            n, false);
                        CHECK(sold.total <= paid.total);
                        e.Trade("b", "cargo", n, false, sold.total);
                    }
                }
        std::cout << "PASS: trade-route profits, travel, reload, finite cash, shrinking margins, "
                     "Speech, local resale and legacy pricing configuration\n";
    } catch (const std::exception &ex) {
        std::cerr << ex.what() << '\n';
        return 1;
    }
}
