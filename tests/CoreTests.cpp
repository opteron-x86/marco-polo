#include "core/Economy.h"
#include <cmath>
#include <fstream>
#include <iostream>
#include <stdexcept>
#define CHECK(x)                                                                                   \
    do {                                                                                           \
        if (!(x))                                                                                  \
            throw std::runtime_error(std::string(__FILE__) + ":" + std::to_string(__LINE__) +      \
                                     " " #x);                                                      \
    } while (false)
using namespace marco;
template <class F> void rejects(F f) {
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
        std::ifstream f(argv[1]);
        auto config = Config::Parse(json::parse(f));
        Economy e(config);
        // Save and RNG state must produce identical catch-up independent of batching.
        e.NewGame(10, 12345);
        CHECK(e.Advance(80, 3) == 3);
        auto snapshot = e.Save();
        Economy other(config);
        other.Load(snapshot);
        while (e.Advance(730, 17)) {
        }
        while (other.Advance(730, 1)) {
        }
        CHECK(e.Save() == other.Save());
        e.Validate();
        auto before = e.Save();
        auto bad = before;
        bad["schema"] = 99;
        rejects([&] { e.Load(bad); });
        CHECK(e.Save() == before);
        bad = before;
        bad["markets"]["whiterun"]["goods"]["iron"]["quantity"] = -1;
        rejects([&] { e.Load(bad); });
        CHECK(e.Save() == before);
        rejects([&] { e.Advance(std::nan("")); });
        rejects([&] { e.Price("whiterun", "iron", 10, -1, true); });
        // Quantities transfer exactly once; sequential marginal rounding matches bulk.
        e.NewGame(0, 4);
        Merchant merchant{"smith", "whiterun", 100000, {}, 0, 0};
        merchant.items["iron"] = {"iron", "iron", 100, 100, 1, 12, false};
        e.RegisterMerchant(merchant);
        auto bulk = e.Price("whiterun", "iron", 12, 20, true).total;
        Money singles = 0;
        for (int i = 0; i < 20; ++i) {
            auto q = e.Price("whiterun", "iron", 12, 1, true);
            singles += q.total;
            e.Trade("smith", "iron", 1, true, q.total);
        }
        CHECK(bulk == singles);
        CHECK(e.Get().merchants.at("smith").items.at("iron").quantity == 80);
        CHECK(e.Multiplier("whiterun", "iron") > 1);
        auto afterBuy = e.Multiplier("whiterun", "iron");
        e.Trade("smith", "iron", 20, false, 100);
        CHECK(e.Multiplier("whiterun", "iron") < afterBuy);
        rejects([&] { e.Trade("smith", "iron", 101, true, 1); });
        // No local round-trip profit, even with a hypothetical zero barter spread.
        for (int n : {1, 10, 100}) {
            auto paid = e.Price("whiterun", "iron", 100, n, true).total;
            e.Trade("smith", "iron", n, true, paid);
            auto returned = e.Price("whiterun", "iron", 100, n, false).total;
            CHECK(returned <= paid);
            e.Trade("smith", "iron", n, false, returned);
        }
        // Reading a settled transaction again must not count its supply twice.
        auto ledgerBefore = e.Get().markets.at("whiterun").goods.at("iron");
        e.RegisterMerchant(e.Get().merchants.at("smith"));
        CHECK(e.Get().markets.at("whiterun").goods.at("iron").imported == ledgerBefore.imported);
        CHECK(e.Get().markets.at("whiterun").goods.at("iron").exported == ledgerBefore.exported);
        // Theft and travelling stock are recorded in the affected regional ledgers.
        auto observed = e.Get().merchants.at("smith");
        observed.items.at("iron").quantity -= 10;
        e.RegisterMerchant(observed);
        CHECK(e.Get().markets.at("whiterun").goods.at("iron").exported ==
              ledgerBefore.exported + 10);
        CHECK(e.Get().markets.at("whiterun").goods.at("iron").price > ledgerBefore.price);
        observed.market = "rift";
        e.RegisterMerchant(observed);
        CHECK(e.Get().markets.at("whiterun").goods.at("iron").exported ==
              ledgerBefore.exported + 100);
        CHECK(e.Get().markets.at("rift").goods.at("iron").imported == 90);
        e.Advance(30);
        e.Validate();
        auto invalidConfig = config;
        invalidConfig.commodities.front().shelfLife = .5;
        rejects([&] { Economy invalid(invalidConfig); });
        // Shop displays are purchasable even with an empty managed chest. A
        // mixed stack must remain purchasable after its chest portion is gone.
        e.NewGame(0, 17);
        Merchant shop{"shop", "whiterun", 1000, {}, 0, 0};
        shop.items["display"] = {"display", "iron", 0, 0, 1, 12, false, 4};
        shop.items["mixed"] = {"mixed", "iron", 2, 0, 1, 12, false, 3};
        e.RegisterMerchant(shop);
        auto displayQuote = e.Price("whiterun", "iron", 12, 2, true);
        auto displayBefore = e.Get().markets.at("whiterun").goods.at("iron");
        e.Trade("shop", "display", 2, true, displayQuote.total, 2);
        CHECK(e.Get().merchants.at("shop").items.at("display").quantity == 0);
        CHECK(e.Get().merchants.at("shop").items.at("display").externalQuantity == 2);
        CHECK(e.Get().merchants.at("shop").cash == 1000 + displayQuote.total);
        CHECK(e.Get().markets.at("whiterun").goods.at("iron").exported == displayBefore.exported + 2);
        CHECK(e.Get().markets.at("whiterun").goods.at("iron").price > displayBefore.price);
        e.Trade("shop", "mixed", 4, true, 48, 2);
        CHECK(e.Get().merchants.at("shop").items.at("mixed").quantity == 0);
        CHECK(e.Get().merchants.at("shop").items.at("mixed").externalQuantity == 1);
        e.Trade("shop", "mixed", 1, true, 12, 1);
        auto exhausted = e.Save();
        rejects([&] { e.Trade("shop", "mixed", 1, true, 12, 1); });
        rejects([&] { e.Trade("shop", "display", 1, false, 12, 1); });
        CHECK(e.Save() == exhausted);
        auto displayAfter = e.Get().markets.at("whiterun").goods.at("iron");
        e.RegisterMerchant(e.Get().merchants.at("shop"));
        CHECK(e.Get().markets.at("whiterun").goods.at("iron").exported == displayAfter.exported);
        other.Load(e.Save());
        CHECK(other.Save() == e.Save());
        // Existing schema-1 saves have no display-stock field. Only this new
        // field is optional; malformed or negative stock must still roll back.
        auto legacy = e.Save();
        for (auto &[key, item] : legacy["merchants"]["shop"]["items"].items())
            item.erase("externalQuantity");
        other.Load(legacy);
        CHECK(other.Get().merchants.at("shop").items.at("display").externalQuantity == 0);
        bad = e.Save();
        bad["merchants"]["shop"]["items"]["display"]["externalQuantity"] = -1;
        before = e.Save();
        rejects([&] { e.Load(bad); });
        CHECK(e.Save() == before);
        e.Advance(7);
        CHECK(e.Get().merchants.at("shop").items.at("display").externalQuantity == 2);
        CHECK(e.Get().merchants.at("shop").items.at("display").quantity == 0);
        CHECK(e.Get().merchants.at("shop").items.at("mixed").quantity == 0);
        e.Validate();
        // Contract reservations, stable terms, exact-once settlement, refunds.
        e.NewGame(0, 9);
        e.Edit().markets.at("winterhold").goods.at("food").quantity = 0;
        auto balance = e.Get().markets.at("winterhold").treasury;
        auto id = e.Offer("winterhold", "food", 40, "test.esm|001234", "Bread", 8);
        CHECK(id > 0);
        auto contract = *e.FindContract(id);
        CHECK(e.Get().markets.at("winterhold").treasury == balance - contract.reward);
        other.Load(e.Save());
        CHECK(other.FindContract(id)->item == "test.esm|001234");
        CHECK(other.FindContract(id)->itemName == "Bread");
        CHECK(e.Accept(id));
        CHECK(!e.Accept(id));
        CHECK(e.Deliver(id, 39) == 0);
        CHECK(e.Deliver(id, 40) == contract.reward);
        CHECK(e.Deliver(id, 40) == 0);
        CHECK(!e.Cancel(id));
        id = e.Offer("winterhold", "food", 30);
        CHECK(id);
        auto reward = e.FindContract(id)->reward;
        balance = e.Get().markets.at("winterhold").treasury;
        CHECK(e.Cancel(id));
        CHECK(e.Get().markets.at("winterhold").treasury == balance + reward);
        CHECK(!e.Cancel(id));
        id = e.Offer("winterhold", "food", 30);
        CHECK(id);
        CHECK(e.Accept(id));
        e.Advance(15);
        CHECK(e.FindContract(id)->status == ContractStatus::expired);
        CHECK(e.Deliver(id, 30) == 0);
        // Sources/sinks/transit conserve aggregate inventory through trade and spoilage.
        e.NewGame(0, 33);
        double initial = 0;
        for (auto &[m, v] : e.Get().markets)
            for (auto &[c, s] : v.goods)
                initial += s.quantity;
        e.Advance(365, 365);
        double stock = 0, net = 0;
        for (auto &[m, v] : e.Get().markets)
            for (auto &[c, s] : v.goods) {
                stock += s.quantity;
                net += s.produced - s.consumed + s.imported - s.exported - s.lost;
            }
        CHECK(std::abs(stock - initial - net) < 1e-6);
        // Five-year stress, bounded catch-up, disruption and recovery, no NaN/overflow.
        e.SetConditions("whiterun", 1, 1);
        while (e.Advance(1825, 32)) {
            e.Validate();
        }
        e.SetConditions("whiterun", 0, 0);
        e.Advance(1850);
        e.Validate();
        e.NewGame(5, 77);
        CHECK(e.Get().merchants.empty());
        CHECK(e.Get().contracts.empty());
        CHECK(e.Get().shipments.empty());
        CHECK(e.Get().day == 5);
        std::cout << "PASS: deterministic simulation, persistence rejection, quantity pricing, "
                     "conservation, contracts, five-year stress, session reset\n";
    } catch (const std::exception &ex) {
        std::cerr << ex.what() << '\n';
        return 1;
    }
}
