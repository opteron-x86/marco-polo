#pragma once
#include "Security.h"
#include <cstdint>
#include <map>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

namespace marco {
using Money = std::int64_t;
using json = nlohmann::json;
inline constexpr int SaveSchema = 4;
struct Commodity {
    std::string id, name;
    double referenceValue{10}, elasticity{0.5}, shelfLife{0};
    bool seasonal{false}, military{false};
};
struct MarketSpec {
    std::string id, name, location;
    double population{1}, winterSeverity{0.3};
    std::map<std::string, double> output, demand;
    // Local production costs and import dependence, before stock scarcity.
    std::map<std::string, double> priceFactors;
};
struct RouteSpec {
    std::string id, from, to;
    double days{2}, capacity{50}, cost{0.04}, risk{0.02};
};
struct Recipe {
    std::string market, output;
    double amount{1};
    std::map<std::string, double> inputs;
};
struct Config {
    std::vector<Commodity> commodities;
    std::vector<MarketSpec> markets;
    std::vector<RouteSpec> routes;
    std::vector<Recipe> recipes;
    double targetDays{10}, minMultiplier{0.4}, maxMultiplier{3.0}, dailyAdjustment{0.12};
    double merchantTurnover{0.08};
    double barterExponent{1}, minimumBarterMargin{0};
    Money treasury{20000};
    SecurityConfig security;
    void Validate() const;
    const Commodity &Good(const std::string &) const;
    const MarketSpec &Market(const std::string &) const;
    double PriceFactor(const std::string &market, const std::string &commodity) const;
    double BarterMultiplier(double nativeMultiplier, bool buying) const;
    static Config Parse(const json &);
};
struct Stock {
    double quantity{}, price{1}, produced{}, consumed{}, imported{}, exported{}, lost{};
};
struct Market {
    std::map<std::string, Stock> goods;
    Money treasury{};
    double war{}, disruption{};
};
struct Shipment {
    std::uint64_t id{};
    std::string route, from, to, commodity;
    double quantity{}, arrives{}, loss{};
};
struct Item {
    std::string form, commodity;
    int quantity{}, target{};
    double units{1}, value{1};
    bool protectedItem{};
    // Goods offered from shop displays or other sources outside the managed chest.
    // Observed and traded, but never replenished or removed by the simulation.
    int externalQuantity{};
};
struct Merchant {
    std::string id, market;
    Money cash{};
    std::map<std::string, Item> items;
    double lastVisit{}, lastRestock{};
};
struct MarketReport {
    std::string market, commodity, condition, text;
    double issued{}, multiplier{};
};
struct State {
    int schema{SaveSchema};
    double day{}, epoch{};
    std::uint64_t rng{1}, nextID{1}, revision{};
    std::map<std::string, Market> markets;
    std::map<std::string, Merchant> merchants;
    std::vector<Shipment> shipments;
    SecurityState security;
};
struct Quote {
    Money total{};
    int quantity{};
    double multiplier{1};
};
class Economy {
  public:
    explicit Economy(Config config);
    void NewGame(double day, std::uint64_t seed);
    // Advances at most budget full days. Repeated calls converge to target.
    unsigned Advance(double targetDay, unsigned budget = 32);
    double Multiplier(const std::string &market, const std::string &commodity,
                      double delta = 0) const;
    Quote Price(const std::string &market, const std::string &commodity, double unitValue,
                int count, bool buying, double units = 1) const;
    void Trade(const std::string &merchant, const std::string &form, int count, bool buying,
               Money actualGold, int fromExternal = 0);
    void RegisterMerchant(Merchant merchant);
    void SetConditions(const std::string &market, double war, double disruption);
    bool ObserveSite(const std::string &site, SiteControl, double observedDay);
    TradeConditions SecurityConditions() const;
    std::vector<MarketReport> Reports(const std::string &market) const;
    json Save() const;
    void Load(const json &);
    void Validate() const;
    const Config &Definitions() const {
        return config_;
    }
    const State &Get() const {
        return state_;
    }
    State &Edit() {
        return state_;
    }

  private:
    Config config_;
    State state_;
    void Tick();
    void Dispatch(const TradeConditions &);
    double Random();
    double Target(const std::string &, const std::string &) const;
    double Total(const std::string &, const std::string &) const;
};
json Encode(const State &);
State Decode(const json &);
} // namespace marco
