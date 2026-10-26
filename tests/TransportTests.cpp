#include "core/Economy.h"
#include <algorithm>
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
bool Near(double a, double b) {
    return std::abs(a - b) < 1e-7;
}
Config Fixture() {
    Config c;
    c.commodities = {{"iron", "Iron", 10, 0}, {"food", "Food", 10, 0}, {"books", "Books", 10, 0}};
    for (const char *id : {"source", "destination", "other"}) {
        MarketSpec m;
        m.id = m.name = id;
        for (const auto &g : c.commodities)
            m.demand[g.id] = 10;
        c.markets.push_back(m);
    }
    c.routes = {{"road", "source", "destination", 3, 30, 0, 0}};
    return c;
}
void StockAfterConsumption(Economy &e, const std::string &market, const std::string &good,
                           double quantity, double price = 1) {
    auto &s = e.Edit().markets.at(market).goods.at(good);
    s.quantity = quantity + 10;
    s.price = price;
}
double Shipped(const Economy &e, const std::string &good, const std::string &route = "") {
    double quantity = 0;
    for (const auto &s : e.Get().shipments)
        if (s.commodity == good && (route.empty() || route == s.route))
            quantity += s.quantity;
    return quantity;
}
void Run() {
    auto c = Fixture();
    Economy fair(c);
    for (const auto &g : c.commodities) {
        StockAfterConsumption(fair, "source", g.id, 160);
        StockAfterConsumption(fair, "destination", g.id, 60);
    }
    fair.Advance(1);
    for (const auto &g : c.commodities)
        CHECK(Near(Shipped(fair, g.id), 10));
    // Large shortages get more space without excluding smaller eligible orders.
    Economy urgent(c);
    StockAfterConsumption(urgent, "source", "iron", 160);
    StockAfterConsumption(urgent, "source", "food", 160);
    StockAfterConsumption(urgent, "destination", "iron", 10);
    StockAfterConsumption(urgent, "destination", "food", 70);
    StockAfterConsumption(urgent, "destination", "books", 100);
    urgent.Advance(1);
    CHECK(Near(Shipped(urgent, "iron"), 27));
    CHECK(Near(Shipped(urgent, "food"), 3));
    // A highly lucrative cargo cannot take the entire road from another good.
    Economy margin(c);
    StockAfterConsumption(margin, "source", "iron", 160, .4);
    StockAfterConsumption(margin, "source", "food", 160);
    StockAfterConsumption(margin, "destination", "iron", 60, 3);
    StockAfterConsumption(margin, "destination", "food", 60);
    StockAfterConsumption(margin, "destination", "books", 100);
    margin.Advance(1);
    CHECK(Near(Shipped(margin, "iron"), 20));
    CHECK(Near(Shipped(margin, "food"), 10));
    // Scarce source stock is shared between roads, with a single export reserve.
    c.routes = {{"a", "source", "destination", 3, 100, 0, 0},
                {"b", "source", "other", 3, 100, 0, 0}};
    Economy source(c);
    StockAfterConsumption(source, "source", "iron", 100);
    StockAfterConsumption(source, "destination", "iron", 0);
    StockAfterConsumption(source, "other", "iron", 0);
    source.Advance(1);
    CHECK(Near(Shipped(source, "iron", "a"), 20));
    CHECK(Near(Shipped(source, "iron", "b"), 20));
    CHECK(Near(source.Get().markets.at("source").goods.at("iron").quantity, 60));
    // Several exporters must not duplicate a destination's outstanding order.
    c.routes = {{"a", "source", "destination", 3, 100, 0, 0},
                {"b", "other", "destination", 3, 100, 0, 0}};
    Economy destination(c);
    StockAfterConsumption(destination, "source", "iron", 160);
    StockAfterConsumption(destination, "other", "iron", 160);
    StockAfterConsumption(destination, "destination", "iron", 70);
    // Twenty units are already expected from an older, partly lost shipment.
    destination.Edit().shipments.push_back({1, "a", "source", "destination", "iron", 25, 3, .2});
    destination.Edit().nextID = 2;
    destination.Advance(1);
    CHECK(Near(Shipped(destination, "iron", "a"), 30));
    CHECK(Near(Shipped(destination, "iron", "b"), 5));
    // Unused shares go to cargo which still has a supplier and a customer.
    c = Fixture();
    c.routes[0].capacity = 50;
    Economy refill(c);
    StockAfterConsumption(refill, "source", "iron", 65);
    StockAfterConsumption(refill, "source", "food", 160);
    StockAfterConsumption(refill, "destination", "iron", 0);
    StockAfterConsumption(refill, "destination", "food", 0);
    StockAfterConsumption(refill, "destination", "books", 100);
    refill.Advance(1);
    CHECK(Near(Shipped(refill, "iron"), 5));
    CHECK(Near(Shipped(refill, "food"), 45));
    Economy costly(c);
    StockAfterConsumption(costly, "source", "iron", 160, 2);
    StockAfterConsumption(costly, "source", "food", 160, 2);
    StockAfterConsumption(costly, "destination", "iron", 49, .5);
    StockAfterConsumption(costly, "destination", "food", 51, .5);
    StockAfterConsumption(costly, "destination", "books", 100);
    costly.Advance(1);
    CHECK(Shipped(costly, "iron") > 0);
    CHECK(Shipped(costly, "food") == 0);
    // Sub-unit cargo is still valid: tight capacity cannot starve all requests.
    c.routes[0].capacity = .01;
    Economy small(c);
    for (const auto &g : c.commodities) {
        StockAfterConsumption(small, "source", g.id, 160);
        StockAfterConsumption(small, "destination", g.id, 60);
    }
    small.Advance(1);
    for (const auto &g : c.commodities)
        CHECK(Near(Shipped(small, g.id), .01 / 3));
}
int main(int argc, char **argv) {
    try {
        CHECK(argc == 2);
        Run();
        std::ifstream file(argv[1]);
        auto config = Config::Parse(json::parse(file));
        auto reordered = config;
        std::reverse(reordered.commodities.begin(), reordered.commodities.end());
        std::reverse(reordered.routes.begin(), reordered.routes.end());
        std::reverse(reordered.markets.begin(), reordered.markets.end());
        Economy a(config), b(reordered);
        a.NewGame(0, 42);
        b.NewGame(0, 42);
        for (const auto &m : config.markets) {
            Merchant shop{m.id, m.id, 2000, {}, 0, 0};
            for (const auto &c : config.commodities)
                shop.items[c.id] = {c.id, c.id, 4, 4, 1, c.referenceValue, false};
            a.RegisterMerchant(shop);
            b.RegisterMerchant(shop);
        }
        for (int day = 1; day <= 90; ++day) {
            a.Advance(day);
            b.Advance(day);
            CHECK(a.Save() == b.Save());
            a.Validate();
        }
        std::cout << "PASS: cargo fairness, shortage priority, shared reserves and orders, "
                     "in-flight supply, redistribution and configuration-order independence\n";
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
