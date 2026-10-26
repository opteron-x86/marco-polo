#include "Economy.h"
#include <algorithm>
#include <cmath>

namespace marco {
void Economy::Retail(std::map<std::pair<std::string, std::string>, DemandSample> &demand) {
    using Key = std::pair<std::string, std::string>;
    struct Request {
        Merchant *merchant;
        Item *item;
        double cost{}, fill{};
    };
    std::vector<Request> requests;
    std::map<Key, double> needs, smallest;
    // Retail customers are part of regional consumption. Observing more shops
    // must not create another independent population of customers for each shop.
    for (auto &[id, merchant] : state_.merchants) {
        for (auto &[form, item] : merchant.items) {
            if (item.protectedItem)
                continue;
            auto key = Key{merchant.market, item.commodity};
            auto &d = demand.at(key);
            auto &stock = state_.markets.at(merchant.market).goods.at(item.commodity);
            double expected = item.quantity * config_.merchantTurnover;
            int sales = static_cast<int>(std::floor(expected));
            // Preserve the fractional rate: eight items at 8% turnover have
            // an expected .64 sales, not an 8% chance of selling one item.
            double fraction = expected - sales;
            if (fraction > 0 && Random() < fraction)
                ++sales;
            sales = std::min(sales, static_cast<int>(std::min(
                                        100000.0, std::floor(std::max(0.0, d.wanted - d.supplied) /
                                                             item.units))));
            item.quantity -= sales;
            stock.consumed += sales * item.units;
            d.supplied += sales * item.units;
            merchant.cash +=
                static_cast<Money>(std::min(static_cast<double>(1000000000 - merchant.cash),
                                            std::floor(sales * item.value * .5 * stock.price)));
            double cost = std::max(1.0, item.value * .35 * stock.price);
            int need =
                std::min(std::max(0, item.target - item.quantity),
                         static_cast<int>(std::min(100000.0, std::floor(merchant.cash / cost))));
            if (need > 0) {
                needs[key] += need * item.units;
                auto it = smallest.find(key);
                if (it == smallest.end() || item.units < it->second)
                    smallest[key] = item.units;
                requests.push_back(
                    {&merchant, &item, cost, static_cast<double>(item.quantity) / item.target});
            }
        }
        merchant.lastRestock = state_.day;
    }
    for (auto &[key, d] : demand) {
        auto &stock = state_.markets.at(key.first).goods.at(key.second);
        // Hold a bounded retail share before direct household consumption. One
        // indivisible item may accumulate over several scarce days. Otherwise a
        // warehouse producing less than local demand can never restock a shop.
        double reserve = needs.contains(key)
                             ? std::min(needs.at(key), std::max(smallest.at(key), d.wanted * .25))
                             : 0;
        double used =
            std::min(std::max(0.0, stock.quantity - reserve), std::max(0.0, d.wanted - d.supplied));
        stock.quantity -= used;
        stock.consumed += used;
        d.supplied += used;
        if (statistics_)
            statistics_->demand.push_back(d);
    }
    // Replenish the most depleted offers first. Stable IDs break equal ties;
    // the fill fraction changes next day, so a full first-listed offer cannot
    // permanently take supply from an empty later-listed offer.
    std::sort(requests.begin(), requests.end(), [](const auto &a, const auto &b) {
        if (a.fill != b.fill)
            return a.fill < b.fill;
        return std::pair(a.merchant->id, a.item->form) < std::pair(b.merchant->id, b.item->form);
    });
    for (const auto &request : requests) {
        auto &merchant = *request.merchant;
        auto &item = *request.item;
        auto &market = state_.markets.at(merchant.market);
        auto &stock = market.goods.at(item.commodity);
        int delivery = std::min(
            {std::max(0, item.target - item.quantity),
             static_cast<int>(std::min(100000.0, std::floor(stock.quantity / item.units))),
             static_cast<int>(std::min(100000.0, std::floor(merchant.cash / request.cost)))});
        item.quantity += delivery;
        stock.quantity -= delivery * item.units;
        Money paid = static_cast<Money>(std::ceil(delivery * request.cost));
        merchant.cash -= paid;
        market.treasury = std::min(config_.treasury, market.treasury + paid);
    }
    for (auto &[id, market] : state_.markets)
        for (const auto &good : config_.commodities)
            if (good.shelfLife > 0) {
                auto &stock = market.goods.at(good.id);
                double loss = stock.quantity / good.shelfLife;
                stock.quantity -= loss;
                stock.lost += loss;
            }
}
} // namespace marco
