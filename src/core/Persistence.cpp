#include "Economy.h"
#include <algorithm>
#include <stdexcept>
namespace marco {
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Stock, quantity, price, produced, consumed, imported, exported,
                                   lost)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Market, goods, treasury, war, disruption)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Shipment, id, route, from, to, commodity, quantity, arrives,
                                   loss)
void to_json(json &j, const Item &i) {
    j = {{"form", i.form},
         {"commodity", i.commodity},
         {"quantity", i.quantity},
         {"target", i.target},
         {"units", i.units},
         {"value", i.value},
         {"protectedItem", i.protectedItem},
         {"externalQuantity", i.externalQuantity}};
}
void from_json(const json &j, Item &i) {
    j.at("form").get_to(i.form);
    j.at("commodity").get_to(i.commodity);
    j.at("quantity").get_to(i.quantity);
    j.at("target").get_to(i.target);
    j.at("units").get_to(i.units);
    j.at("value").get_to(i.value);
    j.at("protectedItem").get_to(i.protectedItem);
    // Schema-1 saves from 0.1.0–0.1.2 tracked only the chest.
    i.externalQuantity = j.value("externalQuantity", 0);
}
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Merchant, id, market, cash, items, lastVisit, lastRestock)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(State, schema, day, epoch, rng, nextID, revision, markets,
                                   merchants, shipments, security)
json Encode(const State &s) {
    return json(s);
}
State Decode(const json &j) {
    auto schema = j.at("schema").get<int>();
    if (schema < 1 || schema > SaveSchema)
        throw std::runtime_error("Unsupported Marco Polo save schema");
    if (schema < 4 && !j.contains("security")) {
        auto migrated = j;
        migrated["security"] = SecurityState{};
        return migrated.get<State>();
    }
    return j.get<State>();
}
json Economy::Save() const {
    Validate();
    return Encode(state_);
}
void Economy::Load(const json &j) {
    State incoming = Decode(j);
    if (incoming.schema == 1) {
        // 0.1.4 introduces household goods. Seed only this new market stock;
        // preserve existing prices, merchants, time and RNG state.
        // Missing established goods or partially missing household goods still
        // indicate incompatible data and must not silently become a new market.
        if (std::any_of(config_.commodities.begin(), config_.commodities.end(),
                        [](const auto &good) { return good.id == "housewares"; })) {
            std::size_t missing = 0;
            for (auto &spec : config_.markets)
                missing += !incoming.markets.at(spec.id).goods.contains("housewares");
            if (missing) {
                if (missing != config_.markets.size())
                    throw std::runtime_error("Incomplete legacy household goods market");
                for (auto &[id, merchant] : incoming.merchants)
                    for (auto &[form, item] : merchant.items)
                        if (item.commodity == "housewares")
                            throw std::runtime_error("Legacy goods missing for tracked item");
                for (auto &shipment : incoming.shipments)
                    if (shipment.commodity == "housewares")
                        throw std::runtime_error("Legacy goods missing for shipment");
                for (auto &spec : config_.markets)
                    incoming.markets.at(spec.id).goods.emplace(
                        "housewares", Stock{.quantity = Target(spec.id, "housewares")});
            }
        }
    }
    // Schema 4 adds dated security observations. Older markets, stocks, RNG and
    // shipments are retained exactly; no historical location clears are invented.
    incoming.schema = SaveSchema;
    auto old = std::move(state_);
    state_ = std::move(incoming);
    try {
        Validate();
    } catch (...) {
        state_ = std::move(old);
        throw;
    }
}
} // namespace marco
