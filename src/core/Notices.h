#pragma once
#include "Economy.h"
#include <charconv>

namespace marco {
inline std::string OrderPaperName(std::string_view title, std::uint64_t id) {
    return std::string(title) + " #" + std::to_string(id);
}
inline std::uint64_t OrderPaperID(std::string_view title, std::string_view name) {
    auto separator = name.rfind(" #");
    if (separator == std::string_view::npos || name.substr(0, separator) != title)
        return 0;
    std::uint64_t id{};
    auto number = name.substr(separator + 2);
    auto [end, error] = std::from_chars(number.data(), number.data() + number.size(), id);
    return error == std::errc{} && end == number.data() + number.size() &&
                   name == OrderPaperName(title, id)
               ? id
               : 0;
}
// Board postings and carried receipts are separate forms. Only an accepted
// order earns a receipt; old postings never become a new order in the pack.
struct NoticeInventory {
    int bulletinOnBoard{1};
    int orderOnBoard{};
    int receiptInPack{};
    int ReceiptsToKeep(int existing, bool issueMissing) const {
        return receiptInPack && (existing > 0 || issueMissing) ? 1 : 0;
    }
};
inline NoticeInventory NoticesFor(const Contract *contract) {
    bool live = contract && (contract->status == ContractStatus::offered ||
                             contract->status == ContractStatus::accepted);
    return {1, live ? 1 : 0, live && contract->status == ContractStatus::accepted ? 1 : 0};
}
inline bool BoardActionAllowed(const Contract *contract, std::uint64_t displayedID, int action,
                               bool atIssuingBoard) {
    if (!atIssuingBoard || !contract || contract->id != displayedID)
        return false;
    return action == 0
               ? contract->status == ContractStatus::offered
               : (action == 1 || action == 2) && contract->status == ContractStatus::accepted;
}
} // namespace marco
