#include "Economy.h"
#include <stdexcept>
namespace marco {
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Stock, quantity, price, produced, consumed, imported, exported,
                                   lost)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Market, goods, treasury, war, disruption)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Shipment, id, route, from, to, commodity, quantity, arrives,
                                   loss)
void to_json(json &j, const Item &i) {
    j = {{"form", i.form}, {"commodity", i.commodity}, {"quantity", i.quantity},
         {"target", i.target}, {"units", i.units}, {"value", i.value},
         {"protectedItem", i.protectedItem}, {"externalQuantity", i.externalQuantity}};
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
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Contract, id, market, commodity, quantity, reward, issued,
                                   deadline, status, item, itemName)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(State, schema, day, epoch, rng, nextID, revision, markets,
                                   merchants, shipments, contracts)
json Encode(const State &s) {
    return json(s);
}
State Decode(const json &j) {
    if (j.at("schema").get<int>() != 1)
        throw std::runtime_error("Unsupported Marco Polo save schema");
    return j.get<State>();
}
json Economy::Save() const {
    Validate();
    return Encode(state_);
}
void Economy::Load(const json &j) {
    State incoming = Decode(j);
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
