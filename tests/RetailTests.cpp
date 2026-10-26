#include "core/Economy.h"
#include <cmath>
#include <iostream>
#include <stdexcept>
using namespace marco;
void Check(bool good, const char *why) {
    if (!good)
        throw std::runtime_error(why);
}
Config Fixture(double output = 10, double demand = 10) {
    Config c;
    c.commodities = {{"iron", "Iron", 10, 0}};
    c.markets = {{"hold", "Hold", "", 1, 0, {{"iron", output}}, {{"iron", demand}}, {}}};
    return c;
}
Merchant Shop(const std::string &id, int count, int target) {
    Merchant m{id, "hold", 100000, {}, 0, 0};
    m.items["iron"] = {"iron", "iron", count, target, 1, 10, false};
    return m;
}
int main() {
    try {
        auto c = Fixture();
        c.merchantTurnover = 1;
        Economy one(c), many(c);
        one.RegisterMerchant(Shop("a", 100, 100));
        for (int i = 0; i < 10; ++i)
            many.RegisterMerchant(Shop(std::to_string(i), 100, 100));
        for (auto *e : {&one, &many}) {
            e->CollectStatistics(true);
            e->Advance(1);
            const auto &d = e->Statistics()->demand.front();
            Check(d.wanted == 10 && d.supplied == 10,
                  "Shop customers changed total regional demand");
            Check(e->Get().markets.at("hold").goods.at("iron").consumed == 10,
                  "Regional and retail demand counted twice");
        }
        // Fractional turnover has the same expected rate whether eight units
        // occupy one row or eight rows. Targets are zero to observe actual sales.
        double whole = 0, split = 0;
        c = Fixture(0, 1000);
        for (std::uint64_t seed = 1; seed <= 2048; ++seed) {
            Economy a(c), b(c);
            a.NewGame(0, seed * 0x9E3779B97F4A7C15ULL);
            b.NewGame(0, seed * 0x9E3779B97F4A7C15ULL);
            a.RegisterMerchant(Shop("shop", 8, 0));
            auto m = Shop("shop", 0, 0);
            m.items.clear();
            for (int i = 0; i < 8; ++i) {
                auto id = std::to_string(i);
                m.items[id] = {id, "iron", 1, 0, 1, 10, false};
            }
            b.RegisterMerchant(m);
            a.Advance(1);
            b.Advance(1);
            whole += 8 - a.Get().merchants.at("shop").items.at("iron").quantity;
            for (const auto &[id, item] : b.Get().merchants.at("shop").items)
                split += 1 - item.quantity;
        }
        whole /= 2048;
        split /= 2048;
        Check(std::abs(whole - .64) < .04 && std::abs(split - .64) < .04,
              "Incorrect fractional turnover expectation");
        // Production below household demand must eventually reach shop shelves.
        c = Fixture(.5, 10);
        c.merchantTurnover = .2;
        Economy scarce(c);
        scarce.Edit().markets.at("hold").goods.at("iron").quantity = 0;
        scarce.RegisterMerchant(Shop("shop", 0, 5));
        scarce.CollectStatistics(true);
        scarce.Advance(1);
        Check(scarce.Get().markets.at("hold").goods.at("iron").quantity > 0,
              "Households consumed reserved fractional supply");
        auto saved = scarce.Save();
        Economy restored(c);
        restored.Load(saved);
        scarce.Advance(2);
        restored.Advance(2);
        Check(scarce.Save() == restored.Save(), "Fractional reserve changed after reload");
        Check(scarce.Get().merchants.at("shop").items.at("iron").quantity > 0,
              "Scarce merchant never received stock");
        int stockedDays = 0;
        for (int day = 3; day <= 90; ++day) {
            scarce.Advance(day);
            stockedDays += scarce.Get().merchants.at("shop").items.at("iron").quantity > 0;
            scarce.Validate();
        }
        Check(stockedDays > 70, "Shop remains chronically empty despite recurring supply");
        // An empty later-listed offer receives supply before a half-full one.
        c = Fixture(0, 10);
        c.merchantTurnover = 0;
        Economy priority(c);
        priority.Edit().markets.at("hold").goods.at("iron").quantity = 1;
        auto m = Shop("shop", 5, 10);
        m.items["z"] = {"z", "iron", 0, 10, 1, 10, false};
        priority.RegisterMerchant(m);
        priority.Advance(1);
        Check(priority.Get().merchants.at("shop").items.at("z").quantity == 1,
              "Depleted offer lost its restock priority");
        // Unfunded, protected and display stock must not reserve warehouse goods.
        Economy noCash(c);
        noCash.Edit().markets.at("hold").goods.at("iron").quantity = 5;
        m = Shop("shop", 0, 10);
        m.cash = 0;
        m.items["iron"].externalQuantity = 4;
        m.items["quest"] = {"quest", "iron", 0, 100, 1, 10, true};
        noCash.RegisterMerchant(m);
        noCash.Advance(1);
        Check(noCash.Get().markets.at("hold").goods.at("iron").quantity == 0,
              "Invalid shop order reserved goods");
        Check(noCash.Get().merchants.at("shop").items.at("iron").externalQuantity == 4,
              "Display stock changed");
        Check(noCash.Get().merchants.at("shop").items.at("quest").quantity == 0,
              "Protected stock changed");
        std::cout << "PASS: single regional demand, fractional turnover, scarce restocking, "
                     "priority, reload and protected stock\n";
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
