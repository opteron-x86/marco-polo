#pragma once
#include "Economy.h"

namespace marco {
// Player information is independent of the economy. Reports contain values,
// never references back to a live market.
struct TradeReport {
    std::string market, commodity, source;
    double observed{}, learned{}, low{}, high{};
    bool rumor{};
};
struct MarketSample {
    double day{};
    std::map<std::string, std::map<std::string, double>> rates;
};
struct RumorBatch {
    double heard{};
    std::vector<TradeReport> reports;
};
struct PriceGap {
    TradeReport buy, sell;
    double ratio{};
};
class Knowledge {
  public:
    static constexpr std::size_t HistoryLimit = 32;
    void Reset();
    void Capture(const Economy &);
    unsigned Advance(Economy &, double target, unsigned budget);
    void Observe(const Economy &, const std::string &market, double now, const std::string &source);
    // All innkeepers in a hold share one daily batch. Asking again, changing
    // innkeepers or reloading cannot reroll the news or refresh its dates.
    RumorBatch Hear(const Economy &, const std::string &market, double now,
                    const std::string &source);
    const std::map<std::string, std::map<std::string, TradeReport>> &Reports() const {
        return reports_;
    }
    std::vector<PriceGap> Opportunities() const;
    std::vector<TradeReport> RecentRumors() const;
    json Save() const;
    void Load(const json &, const Config &, double now);
    static std::string RumorText(const TradeReport &, const Config &);

  private:
    std::vector<MarketSample> history_;
    std::map<std::string, std::map<std::string, TradeReport>> reports_;
    std::map<std::string, RumorBatch> batches_;
    void Learn(const TradeReport &);
};
} // namespace marco
