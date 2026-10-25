#include "Knowledge.h"
#include <algorithm>
#include <cmath>
#include <set>
#include <stdexcept>

namespace marco {
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(TradeReport, market, commodity, source, observed, learned, low,
                                   high, rumor)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(MarketSample, day, rates)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(RumorBatch, heard, reports)
namespace {
void Require(bool ok, const char *message) {
    if (!ok)
        throw std::runtime_error(message);
}
std::uint64_t Hash(const std::string &s) {
    std::uint64_t h = 14695981039346656037ULL;
    for (unsigned char c : s) {
        h ^= c;
        h *= 1099511628211ULL;
    }
    return h;
}
bool Time(double t, double now) {
    return std::isfinite(t) && t >= 0 && t <= now + .001;
}
TradeReport Rumor(const Config &config, const std::string &market, const std::string &good,
                  double rate, double observed, double now, const std::string &source) {
    // Rumors provide a band, not a precise quote leaked from the simulation.
    double low = std::max(config.minMultiplier, std::floor(rate * 5) / 5);
    double high = std::min(config.maxMultiplier, low + .2);
    return {market, good, source, observed, now, low, high, true};
}
} // namespace
void Knowledge::Reset() {
    history_.clear();
    reports_.clear();
    batches_.clear();
}
void Knowledge::Capture(const Economy &e) {
    // One immutable sample per simulated day. UI visits and purchases never
    // rewrite a report which may already be travelling to another hold.
    if (!history_.empty() && history_.back().day >= e.Get().day)
        return;
    MarketSample s;
    s.day = e.Get().day;
    for (auto &[market, state] : e.Get().markets)
        for (auto &[good, stock] : state.goods)
            s.rates[market][good] = stock.price;
    history_.push_back(std::move(s));
    if (history_.size() > HistoryLimit)
        history_.erase(history_.begin());
}
unsigned Knowledge::Advance(Economy &e, double target, unsigned budget) {
    Require(std::isfinite(target) && target >= 0 && budget <= 100000,
            "Invalid information advance");
    Capture(e);
    unsigned steps = 0;
    while (steps < budget && e.Advance(target, 1)) {
        Capture(e);
        ++steps;
    }
    return steps;
}
void Knowledge::Learn(const TradeReport &r) {
    auto &market = reports_[r.market];
    auto old = market.find(r.commodity);
    // Barter pauses the calendar. A later firsthand observation at the same
    // timestamp can still contain a newly changed price.
    if (old == market.end() || r.observed > old->second.observed ||
        (r.observed == old->second.observed && !r.rumor))
        market[r.commodity] = r;
}
void Knowledge::Observe(const Economy &e, const std::string &market, double now,
                        const std::string &source) {
    Require(Time(e.Get().day, now) && now < e.Get().day + 1 && !source.empty() &&
                source.size() <= 200,
            "Cannot observe an out-of-date market");
    for (auto &[good, stock] : e.Get().markets.at(market).goods)
        Learn({market, good, source, now, now, stock.price, stock.price, false});
}
RumorBatch Knowledge::Hear(const Economy &e, const std::string &market, double now,
                           const std::string &source) {
    auto &config = e.Definitions();
    config.Market(market);
    Require(Time(e.Get().day, now) && now < e.Get().day + 1 && !source.empty() &&
                source.size() <= 200,
            "Cannot hear news from an out-of-date market");
    auto previous = batches_.find(market);
    if (previous != batches_.end() && std::floor(previous->second.heard) == std::floor(now))
        return previous->second;
    Capture(e);
    // News travels along incoming routes. Use their actual travel time and
    // history from before departure; never fabricate prices before initialization.
    std::map<std::string, double> delays;
    for (auto &r : config.routes)
        if (r.to == market && r.from != market) {
            auto [it, added] = delays.emplace(r.from, std::max(1.0, r.days));
            if (!added)
                it->second = std::min(it->second, std::max(1.0, r.days));
        }
    struct Candidate {
        TradeReport report;
        double score;
    };
    std::vector<Candidate> candidates;
    for (auto &[origin, delay] : delays) {
        auto sample = std::find_if(history_.rbegin(), history_.rend(),
                                   [&](auto &s) { return s.day <= now - delay; });
        if (sample == history_.rend() || now - sample->day > 30)
            continue;
        for (auto &[good, rate] : sample->rates.at(origin)) {
            auto knownMarket = reports_.find(origin);
            const TradeReport *known = nullptr;
            if (knownMarket != reports_.end()) {
                auto it = knownMarket->second.find(good);
                if (it != knownMarket->second.end())
                    known = &it->second;
            }
            // Do not present an old rumor as an update to fresher firsthand news.
            if (known && known->observed >= sample->day)
                continue;
            double variety = (Hash(market + origin + good +
                                   std::to_string(static_cast<std::int64_t>(std::floor(now)))) %
                              1000) /
                             1000.0;
            candidates.push_back({Rumor(config, origin, good, rate, sample->day, now, source),
                                  std::abs(std::log(rate)) + variety * .65});
        }
    }
    std::sort(candidates.begin(), candidates.end(), [](auto &a, auto &b) {
        if (a.score != b.score)
            return a.score > b.score;
        return std::pair(a.report.market, a.report.commodity) <
               std::pair(b.report.market, b.report.commodity);
    });
    RumorBatch batch{now, {}};
    std::map<std::string, int> origins;
    std::set<std::string> goods;
    for (auto &c : candidates) {
        if (origins[c.report.market] == 2 || goods.contains(c.report.commodity))
            continue;
        batch.reports.push_back(c.report);
        ++origins[c.report.market];
        goods.insert(c.report.commodity);
        if (batch.reports.size() == 3)
            break;
    }
    // Early in a new game there is no distant history yet. The innkeeper can
    // still discuss today's local conditions without inventing earlier news.
    if (batch.reports.empty()) {
        std::vector<std::pair<double, std::string>> local;
        for (auto &[good, stock] : e.Get().markets.at(market).goods)
            local.emplace_back(std::abs(std::log(stock.price)), good);
        std::sort(local.rbegin(), local.rend());
        for (std::size_t i = 0; i < std::min<std::size_t>(2, local.size()); ++i) {
            auto &good = local[i].second;
            batch.reports.push_back(Rumor(config, market, good,
                                          e.Get().markets.at(market).goods.at(good).price, now, now,
                                          source));
        }
    }
    for (auto &r : batch.reports)
        Learn(r);
    batches_[market] = batch;
    return batch;
}
std::vector<PriceGap> Knowledge::Opportunities() const {
    std::map<std::string, std::vector<TradeReport>> byGood;
    for (auto &[market, goods] : reports_)
        for (auto &[good, report] : goods)
            byGood[good].push_back(report);
    std::vector<PriceGap> out;
    for (auto &[good, reports] : byGood) {
        PriceGap best;
        for (auto &buy : reports)
            for (auto &sell : reports) {
                if (buy.market == sell.market)
                    continue;
                double ratio = sell.low / buy.high;
                if (ratio > 1.5 && ratio > best.ratio)
                    best = {buy, sell, ratio};
            }
        if (best.ratio)
            out.push_back(best);
    }
    std::sort(out.begin(), out.end(), [](auto &a, auto &b) {
        return a.ratio != b.ratio ? a.ratio > b.ratio : a.buy.commodity < b.buy.commodity;
    });
    if (out.size() > 5)
        out.resize(5);
    return out;
}
std::vector<TradeReport> Knowledge::RecentRumors() const {
    std::vector<TradeReport> out;
    for (auto &[market, batch] : batches_)
        out.insert(out.end(), batch.reports.begin(), batch.reports.end());
    std::stable_sort(out.begin(), out.end(),
                     [](auto &a, auto &b) { return a.learned > b.learned; });
    return out;
}
std::string Knowledge::RumorText(const TradeReport &r, const Config &config) {
    std::string goods = config.Good(r.commodity).name;
    if (!goods.empty() && goods.front() >= 'A' && goods.front() <= 'Z')
        goods.front() += 'a' - 'A';
    double rate = (r.low + r.high) / 2;
    return "Word from " + config.Market(r.market).name + ": prices for " + goods +
           (rate >= 1.3  ? " were high."
            : rate <= .8 ? " were low."
                         : " were about usual.");
}
json Knowledge::Save() const {
    return {{"schema", 1}, {"history", history_}, {"reports", reports_}, {"batches", batches_}};
}
void Knowledge::Load(const json &j, const Config &config, double now) {
    Require(std::isfinite(now) && now >= 0 && j.at("schema").get<int>() == 1,
            "Unsupported market information schema");
    Knowledge incoming;
    incoming.history_ = j.at("history").get<decltype(history_)>();
    incoming.reports_ = j.at("reports").get<decltype(reports_)>();
    incoming.batches_ = j.at("batches").get<decltype(batches_)>();
    Require(incoming.history_.size() <= HistoryLimit &&
                incoming.reports_.size() <= config.markets.size() &&
                incoming.batches_.size() <= config.markets.size(),
            "Invalid market information dimensions");
    auto rateOK = [&](double rate) {
        return std::isfinite(rate) && rate >= config.minMultiplier - .0001 &&
               rate <= config.maxMultiplier + .0001;
    };
    double last = -1;
    for (auto &s : incoming.history_) {
        Require(Time(s.day, now) && s.day > last && s.rates.size() == config.markets.size(),
                "Invalid news history");
        last = s.day;
        for (auto &market : config.markets) {
            auto &goods = s.rates.at(market.id);
            Require(goods.size() == config.commodities.size(), "Invalid historical goods");
            for (auto &good : config.commodities)
                Require(rateOK(goods.at(good.id)), "Invalid historical rate");
        }
    }
    auto check = [&](const TradeReport &r) {
        config.Market(r.market);
        config.Good(r.commodity);
        Require(Time(r.learned, now) && Time(r.observed, r.learned) && rateOK(r.low) &&
                    rateOK(r.high) && r.low <= r.high && (r.rumor || r.low == r.high) &&
                    !r.source.empty() && r.source.size() <= 200,
                "Invalid remembered report");
    };
    for (auto &[market, goods] : incoming.reports_) {
        config.Market(market);
        Require(goods.size() <= config.commodities.size(), "Too many remembered goods");
        for (auto &[good, report] : goods) {
            check(report);
            Require(report.market == market && report.commodity == good,
                    "Remembered report key mismatch");
        }
    }
    for (auto &[market, batch] : incoming.batches_) {
        config.Market(market);
        Require(Time(batch.heard, now) && batch.reports.size() <= 3, "Invalid rumor batch");
        for (auto &r : batch.reports) {
            check(r);
            Require(r.rumor && r.learned == batch.heard, "Invalid rumor date");
        }
    }
    *this = std::move(incoming);
}
} // namespace marco
