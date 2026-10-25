#include "core/Notices.h"
#include <array>
#include <climits>
#include <fstream>
#include <iostream>
#include <stdexcept>

using namespace marco;
#define CHECK(x)                                                                                   \
    do {                                                                                           \
        if (!(x))                                                                                  \
            throw std::runtime_error(std::string(#x) + " at " + std::to_string(__LINE__));         \
    } while (false)

struct Pack {
    int bulletins{12}, postings{4}, receipts{8}, otherMissives{7};
    void Reconcile(const Contract *contract, bool issue = false) {
        auto plan = NoticesFor(contract);
        bulletins = postings = 0;
        receipts = plan.ReceiptsToKeep(receipts, issue);
    }
};
std::uint64_t Offer(Economy &e) {
    e.Edit().markets.at("winterhold").goods.at("food").quantity = 0;
    auto id = e.Offer("winterhold", "food", 30, "Skyrim.esm|000001", "Bread", 8);
    CHECK(id);
    return id;
}
void Lifecycle(const Config &config, int ending) {
    Economy e(config);
    e.NewGame(100, 55);
    auto id = Offer(e);
    Pack pack;
    auto before = e.Save();
    for (int read = 0; read < 25; ++read) {
        auto *c = e.FindContract(id);
        pack.Reconcile(c);
        CHECK(pack.bulletins == 0 && pack.postings == 0 && pack.receipts == 0);
        CHECK(NoticesFor(c).bulletinOnBoard == 1 && NoticesFor(c).orderOnBoard == 1);
    }
    CHECK(e.Save() == before); // Reading cannot reserve more money or accept an order.
    CHECK(!BoardActionAllowed(e.FindContract(id), id, 0, false));
    CHECK(!BoardActionAllowed(e.FindContract(id), id + 1, 0, true));
    CHECK(BoardActionAllowed(e.FindContract(id), id, 0, true));
    CHECK(e.Accept(id));
    pack.Reconcile(e.FindContract(id), true);
    CHECK(pack.receipts == 1 && pack.otherMissives == 7);
    CHECK(!BoardActionAllowed(e.FindContract(id), id, 0, true));
    CHECK(!BoardActionAllowed(e.FindContract(id), id, 1, false));
    CHECK(!BoardActionAllowed(e.FindContract(id), id, 2, false));
    auto accepted = e.Save();
    Economy reloaded(config);
    reloaded.Load(accepted);
    CHECK(reloaded.Save() == accepted);
    Pack migrated; // Old stacks collapse without changing accepted terms.
    migrated.Reconcile(reloaded.FindContract(id), true);
    CHECK(migrated.receipts == 1 && migrated.bulletins == 0 && migrated.postings == 0);
    CHECK(migrated.otherMissives == 7);
    for (int read = 0; read < 25; ++read)
        migrated.Reconcile(reloaded.FindContract(id), true);
    CHECK(migrated.receipts == 1 && reloaded.Save() == accepted);
    migrated.receipts = 0; // Dropping a receipt does not endlessly mint replacements.
    migrated.Reconcile(reloaded.FindContract(id));
    CHECK(migrated.receipts == 0);
    migrated.Reconcile(reloaded.FindContract(id), true); // Issuing board can replace it.
    CHECK(migrated.receipts == 1);
    auto c = *reloaded.FindContract(id);
    CHECK(reloaded.Deliver(id, c.quantity - 1) == 0);
    migrated.Reconcile(reloaded.FindContract(id));
    CHECK(migrated.receipts == 1); // Partial/failed deliveries keep the paper.
    if (ending == 0) {
        CHECK(BoardActionAllowed(reloaded.FindContract(id), id, 1, true));
        CHECK(reloaded.Deliver(id, c.quantity) == c.reward);
        CHECK(reloaded.Deliver(id, c.quantity) == 0);
    } else if (ending == 1) {
        CHECK(BoardActionAllowed(reloaded.FindContract(id), id, 2, true));
        CHECK(reloaded.Cancel(id));
    } else {
        reloaded.Advance(c.deadline + 1, 32);
        CHECK(reloaded.FindContract(id)->status == ContractStatus::expired);
    }
    migrated.Reconcile(reloaded.FindContract(id));
    CHECK(migrated.receipts == 0 && migrated.otherMissives == 7);
    CHECK(NoticesFor(reloaded.FindContract(id)).orderOnBoard == 0);
    for (int action = -1; action <= 3; ++action)
        CHECK(!BoardActionAllowed(reloaded.FindContract(id), id, action, true));
    auto next = Offer(reloaded);
    CHECK(next != id);
    CHECK(!BoardActionAllowed(reloaded.FindContract(next), id, 0, true));
    CHECK(!BoardActionAllowed(reloaded.FindContract(next), id, 1, true));
    migrated.Reconcile(reloaded.FindContract(next));
    CHECK(migrated.receipts == 0); // A new posting never hands out a receipt by itself.
    CHECK(NoticesFor(reloaded.FindContract(next)).orderOnBoard == 1);
    auto save = reloaded.Save();
    e.Load(save);
    CHECK(e.Save() == save);
}
void ReceiptStacks() {
    using Instance = std::pair<std::uint64_t, int>;
    CHECK(OrderPaperStackID(1, {}) == 0);
    const std::array single{Instance{75, 1}};
    CHECK(OrderPaperStackID(1, single) == 75);
    CHECK(OrderPaperStackID(0, single) == 0);
    CHECK(OrderPaperStackID(-1, single) == 0);
    CHECK(OrderPaperStackID(2, single) == 0); // An extra untagged copy is ambiguous.
    const std::array duplicate{Instance{75, 1}, Instance{75, 1}};
    CHECK(OrderPaperStackID(2, duplicate) == 75);
    CHECK(OrderPaperStackID(1, duplicate) == 0);
    const std::array mixed{Instance{75, 1}, Instance{76, 1}};
    CHECK(OrderPaperStackID(2, mixed) == 0); // Never turn an old order into a new one.
    const std::array untagged{Instance{0, 1}, Instance{75, 1}};
    CHECK(OrderPaperStackID(2, untagged) == 0);
    const std::array badCount{Instance{75, -1}};
    CHECK(OrderPaperStackID(1, badCount) == 0);
    const std::array largeCount{Instance{75, INT_MAX}, Instance{75, 1}};
    CHECK(OrderPaperStackID(INT_MAX, largeCount) == 0); // No summation overflow.
}
int main(int argc, char **argv) {
    try {
        CHECK(argc == 2);
        ReceiptStacks();
        constexpr std::string_view title = "The Reach: Accepted supply order";
        for (auto id : {std::uint64_t{1}, std::uint64_t{12345678901234}, UINT64_MAX})
            CHECK(OrderPaperID(title, OrderPaperName(title, id)) == id);
        for (auto name :
             {"The Reach: Accepted supply order", "The Reach: Accepted supply order #",
              "The Reach: Accepted supply order #-1", "The Reach: Accepted supply order #01",
              "The Reach: Accepted supply order #18446744073709551616",
              "The Reach: Accepted supply order #1extra", "Whiterun: Accepted supply order #1"})
            CHECK(OrderPaperID(title, name) == 0);
        std::ifstream stream(argv[1]);
        auto config = Config::Parse(json::parse(stream));
        for (int ending = 0; ending < 3; ++ending)
            Lifecycle(config, ending);
        CHECK(NoticesFor(nullptr).receiptInPack == 0);
        CHECK(!BoardActionAllowed(nullptr, 1, 0, true));
        std::cout << "PASS: notice migration, bounded papers, frozen terms, save/reload, delivery, "
                     "cancellation, expiry and stale actions\n";
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
