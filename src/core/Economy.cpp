#include "Economy.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <set>
#include <stdexcept>

namespace marco {
namespace {
void Require(bool test, const char *why) {
    if (!test)
        throw std::runtime_error(why);
}
double Amount(const std::map<std::string, double> &m, const std::string &k) {
    auto it = m.find(k);
    return it == m.end() ? 0 : it->second;
}
bool Finite(double v) {
    return std::isfinite(v);
}
} // namespace
const Commodity &Config::Good(const std::string &id) const {
    for (auto &c : commodities)
        if (c.id == id)
            return c;
    throw std::runtime_error("Unknown commodity: " + id);
}
const MarketSpec &Config::Market(const std::string &id) const {
    for (auto &m : markets)
        if (m.id == id)
            return m;
    throw std::runtime_error("Unknown market: " + id);
}
void Config::Validate() const {
    Require(!commodities.empty() && commodities.size() <= 128 && !markets.empty() &&
                markets.size() <= 128,
            "Invalid economy dimensions");
    Require(Finite(targetDays) && targetDays >= 1 && targetDays <= 1000, "Invalid target days");
    Require(Finite(minMultiplier) && Finite(maxMultiplier) && minMultiplier > 0 &&
                minMultiplier <= 1 && maxMultiplier >= 1 && maxMultiplier <= 100,
            "Invalid price bounds");
    Require(Finite(dailyAdjustment) && dailyAdjustment > 0 && dailyAdjustment <= 1,
            "Invalid price adjustment");
    Require(Finite(merchantTurnover) && merchantTurnover >= 0 && merchantTurnover <= 1 &&
                treasury >= 0,
            "Invalid merchant policy");
    std::set<std::string> ids;
    for (auto &c : commodities) {
        Require(!c.id.empty() && ids.insert(c.id).second, "Duplicate commodity");
        Require(Finite(c.referenceValue) && c.referenceValue > 0 && c.referenceValue <= 1000000 &&
                    Finite(c.elasticity) && c.elasticity >= 0 && c.elasticity <= 2 &&
                    Finite(c.shelfLife) && (c.shelfLife == 0 || c.shelfLife >= 1),
                "Invalid commodity");
    }
    ids.clear();
    for (auto &m : markets) {
        Require(!m.id.empty() && ids.insert(m.id).second, "Duplicate market");
        Require(Finite(m.winterSeverity) && m.winterSeverity >= 0 && m.winterSeverity <= 1,
                "Invalid climate");
        for (auto *table : {&m.output, &m.demand})
            for (auto &[key, value] : *table) {
                Good(key);
                Require(Finite(value) && value >= 0 && value <= 1000000,
                        "Invalid production or demand");
            }
    }
    ids.clear();
    for (auto &r : routes) {
        Market(r.from);
        Market(r.to);
        Require(r.from != r.to && !r.id.empty() && ids.insert(r.id).second, "Invalid route");
        Require(Finite(r.days) && r.days >= 1 && r.days <= 365 && Finite(r.capacity) &&
                    r.capacity > 0 && r.capacity <= 100000 && Finite(r.risk) && r.risk >= 0 &&
                    r.risk <= 1 && Finite(r.cost) && r.cost >= 0,
                "Invalid route parameters");
    }
    for (auto &r : recipes) {
        Market(r.market);
        Good(r.output);
        Require(Finite(r.amount) && r.amount > 0 && !r.inputs.empty(), "Invalid recipe");
        for (auto &[key, n] : r.inputs) {
            Good(key);
            Require(key != r.output && Finite(n) && n > 0, "Invalid recipe input");
        }
    }
}
Config Config::Parse(const json &j) {
    Require(j.at("schema").get<int>() == 1, "Unsupported economy configuration schema");
    Config c;
    auto p = j.value("policy", json::object());
    c.targetDays = p.value("targetDays", 10.0);
    c.minMultiplier = p.value("minMultiplier", 0.4);
    c.maxMultiplier = p.value("maxMultiplier", 3.0);
    c.dailyAdjustment = p.value("dailyAdjustment", 0.12);
    c.merchantTurnover = p.value("merchantTurnover", 0.08);
    c.treasury = p.value("treasury", Money{20000});
    for (auto &v : j.at("commodities"))
        c.commodities.push_back({v.at("id"), v.at("name"), v.value("referenceValue", 10.0),
                                 v.value("elasticity", 0.5), v.value("shelfLife", 0.0),
                                 v.value("seasonal", false), v.value("military", false)});
    for (auto &v : j.at("markets"))
        c.markets.push_back({v.at("id"), v.at("name"), v.value("location", std::string{}),
                             v.value("population", 1.0), v.value("winterSeverity", 0.3),
                             v.at("output").get<std::map<std::string, double>>(),
                             v.at("demand").get<std::map<std::string, double>>()});
    for (auto &v : j.at("routes"))
        c.routes.push_back({v.at("id"), v.at("from"), v.at("to"), v.value("days", 2.0),
                            v.value("capacity", 50.0), v.value("cost", 0.04),
                            v.value("risk", 0.02)});
    for (auto &v : j.value("recipes", json::array()))
        c.recipes.push_back({v.at("market"), v.at("output"), v.at("amount"),
                             v.at("inputs").get<std::map<std::string, double>>()});
    c.Validate();
    return c;
}
Economy::Economy(Config c) : config_(std::move(c)) {
    config_.Validate();
    NewGame(0, 1);
}
void Economy::NewGame(double day, std::uint64_t seed) {
    Require(Finite(day) && day >= 0, "Invalid game time");
    state_ = {};
    state_.day = std::floor(day);
    state_.epoch = state_.day;
    state_.rng = seed ? seed : 1;
    for (auto &m : config_.markets) {
        auto &market = state_.markets[m.id];
        market.treasury = config_.treasury;
        for (auto &c : config_.commodities)
            market.goods[c.id].quantity = Target(m.id, c.id);
    }
}
double Economy::Random() {
    auto x = state_.rng;
    x ^= x << 13;
    x ^= x >> 7;
    x ^= x << 17;
    state_.rng = x;
    return static_cast<double>(x >> 11) * 0x1.0p-53;
}
double Economy::Target(const std::string &m, const std::string &c) const {
    return std::max(5.0, Amount(config_.Market(m).demand, c) * config_.targetDays);
}
double Economy::Total(const std::string &m, const std::string &c) const {
    double n = state_.markets.at(m).goods.at(c).quantity;
    for (auto &[id, v] : state_.merchants)
        if (v.market == m)
            for (auto &[form, item] : v.items)
                if (item.commodity == c)
                    n += (item.quantity + item.externalQuantity) * item.units;
    return n;
}
double Economy::Multiplier(const std::string &m, const std::string &c, double delta) const {
    auto &good = config_.Good(c);
    auto &stock = state_.markets.at(m).goods.at(c);
    // Daily regional price plus the immediate marginal effect of a proposed trade.
    double current = std::max(0.0, Total(m, c));
    double ratio =
        (current + Target(m, c) * 0.25) / (std::max(0.0, current + delta) + Target(m, c) * 0.25);
    return std::clamp(stock.price * std::pow(ratio, good.elasticity), config_.minMultiplier,
                      config_.maxMultiplier);
}
Quote Economy::Price(const std::string &m, const std::string &c, double value, int count,
                     bool buying, double units) const {
    Require(Finite(value) && value >= 0 && value <= 100000000 && count >= 0 && count <= 100000 &&
                Finite(units) && units > 0,
            "Invalid quote");
    Quote q;
    q.quantity = count;
    // Integrate at each unit's midpoint. Reversing a trade retraces the same
    // prices; splitting a bulk trade does not buy an advantageous quote.
    const double stock = Total(m, c), buffer = Target(m, c) * 0.25,
                 price = state_.markets.at(m).goods.at(c).price;
    const double elasticity = config_.Good(c).elasticity;
    for (int i = 0; i < count; ++i) {
        double delta = (buying ? -1.0 : 1.0) * units * (i + 0.5);
        double mult =
            std::clamp(price * std::pow((stock + buffer) / (std::max(0.0, stock + delta) + buffer),
                                        elasticity),
                       config_.minMultiplier, config_.maxMultiplier);
        q.total +=
            std::max<Money>(buying ? 1 : 0, static_cast<Money>(std::floor(value * mult + 0.5)));
    }
    q.multiplier = Multiplier(m, c);
    return q;
}
void Economy::RegisterMerchant(Merchant m) {
    config_.Market(m.market);
    Require(!m.id.empty() && m.cash >= 0 && m.cash <= 1000000000, "Invalid merchant");
    for (auto &[f, i] : m.items) {
        config_.Good(i.commodity);
        Require(f == i.form && i.quantity >= 0 && i.quantity <= 100000 &&
                    i.externalQuantity >= 0 && i.externalQuantity <= 100000 && i.target >= 0 &&
                    i.target <= 100000 && Finite(i.units) && i.units > 0 && Finite(i.value) &&
                    i.value >= 0,
                "Invalid inventory");
    }
    // Reconciliation includes outside changes and travelling merchants, without
    // counting a confirmed Trade again when its inventory is subsequently read.
    using Pair = std::pair<std::string, std::string>;
    std::map<Pair, double> changes, before;
    auto old = state_.merchants.find(m.id);
    bool existing = old != state_.merchants.end();
    if (existing)
        for (auto &[key, item] : old->second.items)
            changes[{old->second.market, item.commodity}] -=
                (item.quantity + item.externalQuantity) * item.units;
    for (auto &[key, item] : m.items)
        changes[{m.market, item.commodity}] +=
            (item.quantity + item.externalQuantity) * item.units;
    for (auto &[pair, delta] : changes)
        before[pair] = Total(pair.first, pair.second);
    state_.merchants[m.id] = std::move(m);
    for (auto &[pair, delta] : changes) {
        auto &[market, commodity] = pair;
        auto &stock = state_.markets.at(market).goods.at(commodity);
        (delta >= 0 ? stock.imported : stock.exported) += std::abs(delta);
        if (existing && delta != 0) {
            double buffer = Target(market, commodity) * 0.25;
            stock.price = std::clamp(stock.price * std::pow((before.at(pair) + buffer) /
                                                                (Total(market, commodity) + buffer),
                                                            config_.Good(commodity).elasticity),
                                     config_.minMultiplier, config_.maxMultiplier);
        }
    }
    ++state_.revision;
}
void Economy::Trade(const std::string &mid, const std::string &form, int n, bool buy, Money gold,
                    int fromExternal) {
    Require(n > 0 && n <= 100000 && gold >= 0, "Invalid transaction");
    Require(fromExternal >= 0 && fromExternal <= n && (buy || fromExternal == 0),
            "Invalid external stock transfer");
    auto &m = state_.merchants.at(mid);
    auto &item = m.items.at(form);
    Require(!item.protectedItem, "Protected item");
    Require(!buy || (item.quantity >= n - fromExternal &&
                    item.externalQuantity >= fromExternal), "Insufficient merchant stock");
    Require(buy || m.cash >= gold, "Insufficient merchant gold");
    Require(buy ? gold <= 1000000000 - m.cash : item.quantity <= 100000 - n,
            "Transaction exceeds ledger limits");
    double before = Total(m.market, item.commodity);
    item.quantity += buy ? -(n - fromExternal) : n;
    item.externalQuantity -= fromExternal;
    m.cash += buy ? gold : -gold;
    auto &stock = state_.markets.at(m.market).goods.at(item.commodity);
    (buy ? stock.exported : stock.imported) += n * item.units;
    double after = Total(m.market, item.commodity);
    stock.price =
        std::clamp(stock.price * std::pow((before + Target(m.market, item.commodity) * 0.25) /
                                              (after + Target(m.market, item.commodity) * 0.25),
                                          config_.Good(item.commodity).elasticity),
                   config_.minMultiplier, config_.maxMultiplier);
    ++state_.revision;
}
void Economy::SetConditions(const std::string &id, double war, double disruption) {
    Require(Finite(war) && Finite(disruption), "Invalid conditions");
    auto &m = state_.markets.at(id);
    m.war = std::clamp(war, 0.0, 1.0);
    m.disruption = std::clamp(disruption, 0.0, 1.0);
}
unsigned Economy::Advance(double target, unsigned budget) {
    Require(Finite(target) && target >= 0 && budget <= 100000, "Invalid simulation advance");
    unsigned n = 0;
    while (state_.day + 1 <= target && n < budget) {
        state_.day += 1;
        Tick();
        ++n;
    }
    return n;
}
void Economy::Tick() {
    for (auto it = state_.shipments.begin(); it != state_.shipments.end();) {
        if (it->arrives <= state_.day) {
            auto &s = state_.markets.at(it->to).goods.at(it->commodity);
            s.quantity += it->quantity * (1 - it->loss);
            s.imported += it->quantity;
            s.lost += it->quantity * it->loss;
            it = state_.shipments.erase(it);
        } else
            ++it;
    }
    for (auto &c : state_.contracts)
        if ((c.status == ContractStatus::offered || c.status == ContractStatus::accepted) &&
            state_.day > c.deadline) {
            state_.markets.at(c.market).treasury += c.reward;
            c.status = ContractStatus::expired;
        }
    double winter =
        (1 + std::cos(2 * 3.141592653589793 * std::fmod(state_.day + 227, 365.0) / 365.0)) / 2;
    for (auto &def : config_.markets) {
        auto &m = state_.markets.at(def.id);
        // Explicit outside taxation/business income; finite treasury prevents runaway contracts.
        m.treasury =
            std::min(config_.treasury, m.treasury + std::max<Money>(1, config_.treasury / 100));
        for (auto &good : config_.commodities) {
            auto &s = m.goods.at(good.id);
            double output = Amount(def.output, good.id) * (1 - m.disruption * 0.7) *
                            (good.seasonal ? 1 - winter * def.winterSeverity : 1);
            // Production responds to storage: stop accumulating inventory nobody can consume.
            output *= std::clamp(2 - Total(def.id, good.id) / Target(def.id, good.id), 0.0, 1.5);
            s.quantity += output;
            s.produced += output;
            double wanted = Amount(def.demand, good.id) * (good.military ? 1 + m.war : 1) *
                            (good.seasonal ? 1 + winter * 0.15 : 1);
            wanted *= std::pow(std::max(0.1, s.price), -good.elasticity * 0.4);
            double used = std::min(s.quantity, wanted);
            s.quantity -= used;
            s.consumed += used;
            if (good.shelfLife > 0) {
                double loss = s.quantity / good.shelfLife;
                s.quantity -= loss;
                s.lost += loss;
            }
        }
    }
    for (auto &r : config_.recipes) {
        auto &m = state_.markets.at(r.market);
        double batches =
            std::clamp(2 - Total(r.market, r.output) / Target(r.market, r.output), 0.0, 1.0);
        for (auto &[c, n] : r.inputs)
            batches = std::min(batches, m.goods.at(c).quantity / n);
        for (auto &[c, n] : r.inputs) {
            m.goods.at(c).quantity -= n * batches;
            m.goods.at(c).consumed += n * batches;
        }
        m.goods.at(r.output).quantity += r.amount * batches;
        m.goods.at(r.output).produced += r.amount * batches;
    }
    for (auto &[id, v] : state_.merchants) {
        auto &m = state_.markets.at(v.market);
        for (auto &[form, i] : v.items) {
            if (i.protectedItem)
                continue;
            auto &s = m.goods.at(i.commodity);
            int sales = static_cast<int>(std::floor(i.quantity * config_.merchantTurnover));
            if (sales == 0 && i.quantity > 0 && Random() < config_.merchantTurnover)
                sales = 1;
            i.quantity -= sales;
            s.consumed += sales * i.units;
            v.cash += static_cast<Money>(sales * i.value * 0.5);
            int need = std::max(0, i.target - i.quantity);
            double cost = std::max(1.0, i.value * 0.35 * s.price);
            int delivery = std::min(
                {need, static_cast<int>(std::min(100000.0, std::floor(s.quantity / i.units))),
                 static_cast<int>(std::min(100000.0, std::floor(v.cash / cost)))});
            i.quantity += delivery;
            s.quantity -= delivery * i.units;
            Money paid = static_cast<Money>(std::ceil(delivery * cost));
            v.cash -= paid;
            m.treasury = std::min(config_.treasury, m.treasury + paid);
        }
        v.lastRestock = state_.day;
    }
    for (auto &r : config_.routes) {
        double remaining = r.capacity * (1 - std::max(state_.markets.at(r.from).disruption,
                                                      state_.markets.at(r.to).disruption) *
                                                 0.75);
        for (auto &c : config_.commodities) {
            auto &src = state_.markets.at(r.from).goods.at(c.id);
            auto &dst = state_.markets.at(r.to).goods.at(c.id);
            double pending = 0;
            for (auto &s : state_.shipments)
                if (s.to == r.to && s.commodity == c.id)
                    pending += s.quantity * (1 - s.loss);
            double need = std::max(0.0, Target(r.to, c.id) - Total(r.to, c.id) - pending);
            double excess = std::max(0.0, src.quantity - Target(r.from, c.id) * 0.6);
            if (dst.price < src.price * (1 + r.cost) &&
                Total(r.to, c.id) > Target(r.to, c.id) * 0.5)
                continue;
            double qty = std::min({remaining, need, excess});
            if (qty < 0.1)
                continue;
            src.quantity -= qty;
            src.exported += qty;
            remaining -= qty;
            double disruption =
                std::max(state_.markets.at(r.from).disruption, state_.markets.at(r.to).disruption);
            double loss = Random() < r.risk + disruption * 0.1 ? 0.15 + Random() * 0.35 : 0;
            state_.shipments.push_back({state_.nextID++, r.id, r.from, r.to, c.id, qty,
                                        state_.day + r.days * (1 + disruption), loss});
        }
    }
    for (auto &def : config_.markets)
        for (auto &c : config_.commodities) {
            auto &s = state_.markets.at(def.id).goods.at(c.id);
            double target =
                std::clamp(std::pow((Target(def.id, c.id) * 1.25) /
                                        (Total(def.id, c.id) + Target(def.id, c.id) * 0.25),
                                    c.elasticity),
                           config_.minMultiplier, config_.maxMultiplier);
            s.price +=
                std::clamp(target - s.price, -config_.dailyAdjustment, config_.dailyAdjustment);
        }
    // Keep terminal contract history bounded, but never discard an active reservation.
    if (state_.contracts.size() > 512)
        std::erase_if(state_.contracts, [&](const auto &c) {
            return c.status != ContractStatus::accepted && c.status != ContractStatus::offered &&
                   c.deadline < state_.day - 60;
        });
    ++state_.revision;
}
double Economy::Reserved(const std::string &m, const std::string &c) const {
    double n = 0;
    for (auto &job : state_.contracts)
        if (job.market == m && job.commodity == c &&
            (job.status == ContractStatus::offered || job.status == ContractStatus::accepted))
            n += job.quantity;
    return n;
}
std::uint64_t Economy::Offer(const std::string &m, const std::string &c, int maximum,
                             std::string item, std::string itemName, double unitValue) {
    Require(maximum > 0 && maximum <= 1000, "Invalid contract quantity");
    auto &market = state_.markets.at(m);
    auto &good = config_.Good(c);
    double missing = Target(m, c) * 0.75 - Total(m, c) - Reserved(m, c);
    int n = static_cast<int>(std::min<double>(maximum, std::floor(std::max(0.0, missing))));
    if (n < 5 || state_.contracts.size() >= 2048)
        return 0;
    Require(Finite(unitValue) && unitValue >= 0 && unitValue <= 1000000, "Invalid contract value");
    Money reward = static_cast<Money>(std::ceil(
        n * (unitValue > 0 ? unitValue : good.referenceValue) * market.goods.at(c).price * 1.2));
    if (reward > market.treasury)
        return 0;
    market.treasury -= reward;
    auto id = state_.nextID++;
    state_.contracts.push_back({id, m, c, n, reward, state_.day, state_.day + 14,
                                ContractStatus::offered, std::move(item), std::move(itemName)});
    ++state_.revision;
    return id;
}
const Contract *Economy::FindContract(std::uint64_t id) const {
    for (auto &c : state_.contracts)
        if (c.id == id)
            return &c;
    return nullptr;
}
bool Economy::Accept(std::uint64_t id) {
    for (auto &c : state_.contracts)
        if (c.id == id && c.status == ContractStatus::offered && state_.day <= c.deadline) {
            c.status = ContractStatus::accepted;
            ++state_.revision;
            return true;
        }
    return false;
}
bool Economy::Cancel(std::uint64_t id) {
    for (auto &c : state_.contracts)
        if (c.id == id &&
            (c.status == ContractStatus::accepted || c.status == ContractStatus::offered)) {
            state_.markets.at(c.market).treasury += c.reward;
            c.status = ContractStatus::cancelled;
            ++state_.revision;
            return true;
        }
    return false;
}
Money Economy::Deliver(std::uint64_t id, int available) {
    for (auto &c : state_.contracts)
        if (c.id == id && c.status == ContractStatus::accepted && state_.day <= c.deadline &&
            available >= c.quantity) {
            auto &s = state_.markets.at(c.market).goods.at(c.commodity);
            s.quantity += c.quantity;
            s.imported += c.quantity;
            c.status = ContractStatus::fulfilled;
            ++state_.revision;
            return c.reward;
        }
    return 0;
}
std::vector<Bulletin> Economy::Reports(const std::string &m) const {
    std::vector<Bulletin> out;
    auto &market = state_.markets.at(m);
    for (auto &c : config_.commodities) {
        auto &s = market.goods.at(c.id);
        std::string condition = s.price > 1.25 ? "shortage" : s.price < 0.8 ? "surplus" : "steady";
        out.push_back({m, c.id, condition,
                       config_.Market(m).name + ": " + c.name + " supplies are " +
                           (condition == "shortage"  ? "scarce"
                            : condition == "surplus" ? "plentiful"
                                                     : "steady") +
                           ".",
                       state_.day, s.price});
    }
    return out;
}
void Economy::Validate() const {
    Require(state_.schema == 1 && Finite(state_.day) && state_.day >= 0 && state_.rng != 0 &&
                state_.nextID > 0,
            "Invalid save header");
    Require(state_.markets.size() == config_.markets.size() && state_.merchants.size() <= 10000 &&
                state_.shipments.size() <= 100000 && state_.contracts.size() <= 2048,
            "Invalid save dimensions");
    for (auto &spec : config_.markets) {
        auto &m = state_.markets.at(spec.id);
        Require(m.goods.size() == config_.commodities.size() && m.treasury >= 0 && Finite(m.war) &&
                    m.war >= 0 && m.war <= 1 && Finite(m.disruption) && m.disruption >= 0 &&
                    m.disruption <= 1,
                "Invalid market save");
        for (auto &c : config_.commodities) {
            auto &s = m.goods.at(c.id);
            for (double n : {s.quantity, s.produced, s.consumed, s.imported, s.exported, s.lost})
                Require(Finite(n) && n >= -1e-6 && n <= 1e15, "Invalid stock save");
            Require(Finite(s.price) && s.price >= config_.minMultiplier - 1e-8 &&
                        s.price <= config_.maxMultiplier + 1e-8,
                    "Invalid price save");
        }
    }
    for (auto &[id, m] : state_.merchants) {
        config_.Market(m.market);
        Require(!id.empty() && m.id == id && m.cash >= 0 && m.cash <= 1000000000 &&
                    m.items.size() <= 10000,
                "Invalid merchant save");
        for (auto &[f, i] : m.items) {
            config_.Good(i.commodity);
            Require(f == i.form && i.quantity >= 0 && i.quantity <= 100000 &&
                        i.externalQuantity >= 0 && i.externalQuantity <= 100000 && i.target >= 0 &&
                        i.target <= 100000 && Finite(i.units) && i.units > 0 && Finite(i.value) &&
                        i.value >= 0,
                    "Invalid item save");
        }
    }
    std::set<std::uint64_t> ids;
    for (auto &s : state_.shipments) {
        config_.Market(s.from);
        config_.Market(s.to);
        config_.Good(s.commodity);
        Require(s.id > 0 && s.id < state_.nextID && ids.insert(s.id).second && Finite(s.quantity) &&
                    s.quantity >= 0 && Finite(s.arrives) && s.arrives >= state_.day &&
                    Finite(s.loss) && s.loss >= 0 && s.loss <= 1,
                "Invalid shipment save");
    }
    for (auto &c : state_.contracts) {
        config_.Market(c.market);
        config_.Good(c.commodity);
        Require(c.id > 0 && c.id < state_.nextID && ids.insert(c.id).second && c.quantity > 0 &&
                    c.quantity <= 1000 && c.reward >= 0 && c.reward <= 1000000000 &&
                    Finite(c.issued) && Finite(c.deadline) && c.deadline >= c.issued &&
                    static_cast<int>(c.status) >= 0 && static_cast<int>(c.status) <= 4,
                "Invalid contract save");
    }
}
} // namespace marco
