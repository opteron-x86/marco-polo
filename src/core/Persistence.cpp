#include "Economy.h"
#include <stdexcept>
namespace marco {
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Stock, quantity, price, produced, consumed, imported, exported,
                                   lost)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Market, goods, treasury, war, disruption)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Shipment, id, route, from, to, commodity, quantity, arrives,
                                   loss)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Item, form, commodity, quantity, target, units, value,
                                   protectedItem)
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
