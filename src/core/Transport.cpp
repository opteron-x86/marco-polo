#include "Economy.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

namespace marco {
void Economy::Dispatch(const TradeConditions &trade) {
    // Plan every route from one stock snapshot. Exporters share one reserve,
    // importers share one shortfall, and each road has its own daily capacity.
    // Cargo already in transit counts toward the destination's needs.
    using Key = std::pair<std::string, std::string>;
    struct Budget {
        double remaining{}, rate{};
    };
    struct MarketCargo {
        double total{}, target{}, need{};
        std::size_t supply{}, demand{};
    };
    struct Cargo {
        const RouteSpec *route;
        std::string good;
        std::array<std::size_t, 3> budgets;
        double weight{}, quantity{};
    };
    constexpr double epsilon = 1e-8;
    std::map<Key, double> incoming;
    for (const auto &s : state_.shipments)
        incoming[{s.to, s.commodity}] += s.quantity * (1 - s.loss);
    std::vector<Budget> budgets;
    auto budget = [&](double remaining) {
        budgets.push_back({remaining, 0});
        return budgets.size() - 1;
    };
    std::map<Key, MarketCargo> markets;
    for (const auto &[id, market] : state_.markets)
        for (const auto &[good, stock] : market.goods) {
            double target = Target(id, good), total = Total(id, good);
            double need = std::max(0.0, target - total - incoming[{id, good}]);
            markets[{id, good}] = {total, target, need,
                                   budget(std::max(0.0, stock.quantity - target * .6)),
                                   budget(need)};
        }
    // Stable order also fixes shipment IDs and loss rolls when a user reorders
    // the configuration without changing its contents.
    std::vector<const RouteSpec *> routes;
    for (const auto &route : config_.routes)
        routes.push_back(&route);
    std::sort(routes.begin(), routes.end(), [](auto a, auto b) { return a->id < b->id; });
    std::vector<Cargo> cargo;
    for (const auto *r : routes) {
        const auto security =
            trade.routes.contains(r->id) ? trade.routes.at(r->id) : RouteSecurity{};
        const auto &source = state_.markets.at(r->from), &destination = state_.markets.at(r->to);
        double disruption = std::max(source.disruption, destination.disruption);
        auto road = budget(r->capacity * (1 - disruption * .75) * security.capacity);
        for (const auto &[good, stock] : source.goods) {
            const auto &src = markets.at({r->from, good}), &dst = markets.at({r->to, good});
            if (std::min({budgets[road].remaining, budgets[src.supply].remaining, dst.need}) <=
                epsilon)
                continue;
            double margin =
                destination.goods.at(good).price / (stock.price * (1 + r->cost * security.cost));
            // Below half the local reserve, essential replenishment can proceed
            // even without a profitable price gap, as in the previous policy.
            if (margin < 1 && dst.total > dst.target * .5)
                continue;
            // Share space by missing units and proportional shortage. Profitable
            // cargo receives at most twice the weight, so valuables cannot take
            // every wagon while other eligible goods receive nothing.
            double weight =
                dst.need * (dst.need / dst.target) * (1 + std::clamp(margin - 1, 0.0, 1.0));
            cargo.push_back({r, good, {road, src.supply, dst.demand}, weight, 0});
        }
    }
    std::vector<std::size_t> active;
    for (std::size_t i = 0; i < cargo.size(); ++i)
        active.push_back(i);
    // Weighted progressive filling: allocate all active requests together until
    // a road, source or destination is full. Remove those requests and redistribute
    // the remaining capacity. At least one request finishes on every iteration.
    while (!active.empty()) {
        for (auto &b : budgets)
            b.rate = 0;
        for (auto i : active)
            for (auto b : cargo[i].budgets)
                budgets[b].rate += cargo[i].weight;
        double step = std::numeric_limits<double>::infinity();
        std::size_t limiting = 0;
        for (std::size_t b = 0; b < budgets.size(); ++b)
            if (budgets[b].rate > 0 && budgets[b].remaining / budgets[b].rate < step) {
                step = budgets[b].remaining / budgets[b].rate;
                limiting = b;
            }
        for (auto i : active) {
            auto &c = cargo[i];
            double quantity = step * c.weight;
            c.quantity += quantity;
            for (auto b : c.budgets)
                budgets[b].remaining = std::max(0.0, budgets[b].remaining - quantity);
        }
        std::erase_if(active, [&](auto i) {
            return std::any_of(cargo[i].budgets.begin(), cargo[i].budgets.end(), [&](auto b) {
                return b == limiting || budgets[b].remaining <= epsilon;
            });
        });
    }
    for (const auto &c : cargo) {
        if (c.quantity <= epsilon)
            continue;
        const auto &r = *c.route;
        const auto security = trade.routes.contains(r.id) ? trade.routes.at(r.id) : RouteSecurity{};
        auto &src = state_.markets.at(r.from).goods.at(c.good);
        src.quantity -= c.quantity;
        src.exported += c.quantity;
        if (statistics_)
            statistics_->departed += c.quantity;
        double disruption =
            std::max(state_.markets.at(r.from).disruption, state_.markets.at(r.to).disruption);
        double risk = std::clamp(r.risk * security.riskScale + disruption * .1 + security.extraRisk,
                                 0.0, 1.0);
        double loss = Random() < risk ? .15 + Random() * .35 : 0;
        state_.shipments.push_back({state_.nextID++, r.id, r.from, r.to, c.good, c.quantity,
                                    state_.day + r.days * (1 + disruption) * security.delay, loss});
    }
}
} // namespace marco
