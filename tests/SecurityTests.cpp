#include "core/Economy.h"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <iostream>
#include <limits>
#include <stdexcept>

using namespace marco;
#define CHECK(x)                                                                                   \
    do {                                                                                           \
        if (!(x))                                                                                  \
            throw std::runtime_error(std::string(__FILE__) + ":" + std::to_string(__LINE__) +      \
                                     " " #x);                                                      \
    } while (false)
template <class F> void Rejects(F f) {
    bool rejected = false;
    try {
        f();
    } catch (const std::exception &) {
        rejected = true;
    }
    CHECK(rejected);
}
Config Fixture() {
    Config c;
    c.commodities = {{"iron", "Iron", 10, .8}, {"food", "Food", 5, .6}};
    c.markets = {{"source",
                  "Source",
                  "",
                  1,
                  0,
                  {{"iron", 35}, {"food", 20}},
                  {{"iron", 10}, {"food", 10}},
                  {}},
                 {"destination",
                  "Destination",
                  "",
                  1,
                  0,
                  {{"iron", 0}, {"food", 4}},
                  {{"iron", 20}, {"food", 10}},
                  {}}};
    c.routes = {{"road", "source", "destination", 3, 40, .04, .03}};
    c.security.enabled = true;
    c.security.sites = {{"camp",
                         "Roadside camp",
                         "destination",
                         SiteKind::Bandit,
                         SiteKind::Unknown,
                         1.8,
                         0,
                         {{"road", 1}},
                         {}},
                        {"crypt",
                         "Crypt",
                         "destination",
                         SiteKind::Crypt,
                         SiteKind::Unknown,
                         4,
                         0,
                         {{"road", 1}},
                         {}},
                        {"fort",
                         "Fort",
                         "destination",
                         SiteKind::Fort,
                         SiteKind::Bandit,
                         1.2,
                         1,
                         {{"road", 1}},
                         {{"food", .3}}}};
    c.Validate();
    return c;
}
void Conservation(const Economy &e, double initial) {
    double stock = 0, net = 0;
    for (const auto &[id, market] : e.Get().markets)
        for (const auto &[good, s] : market.goods) {
            stock += s.quantity;
            net += s.produced - s.consumed + s.imported - s.exported - s.lost;
        }
    CHECK(std::abs(stock - initial - net) < 1e-6);
}
void Run() {
    CHECK(ClassifySite({"LocTypeBanditCamp", "LocSetCave"}) == SiteKind::Bandit);
    CHECK(ClassifySite({"LocTypeMine"}) == SiteKind::Unknown);
    CHECK(ClassifySite({"LocTypeDraugrCrypt", "LocSetNordicRuin"}) == SiteKind::Crypt);
    CHECK(ClassifySite({"LocTypeForswornCamp", "LocSetMilitaryFort"}) == SiteKind::Forsworn);
    CHECK(ClassifySite({"LocTypeMilitaryFort", "CWFort"}) == SiteKind::Fort);
    CHECK(ClassifySite({"LocTypeForswornCamp", "LocTypeBanditCamp"}) == SiteKind::Unknown);
    CHECK(ReadSiteControl(SiteKind::Crypt, true, SiteKind::Unknown) == SiteControl::Unknown);
    CHECK(ReadSiteControl(SiteKind::Bandit, false, SiteKind::Unknown) == SiteControl::Hostile);
    CHECK(ReadSiteControl(SiteKind::Bandit, true, SiteKind::Unknown) == SiteControl::Cleared);
    CHECK(ReadSiteControl(SiteKind::Fort, false, SiteKind::Bandit, -2, 1) == SiteControl::Hostile);
    CHECK(ReadSiteControl(SiteKind::Fort, true, SiteKind::Bandit, -2, 1) == SiteControl::Cleared);
    CHECK(ReadSiteControl(SiteKind::Fort, true, SiteKind::Bandit, 0, 1) == SiteControl::Imperial);
    CHECK(ReadSiteControl(SiteKind::Fort, false, SiteKind::Bandit, 0, 2) ==
          SiteControl::Stormcloak);
    CHECK(ReadSiteControl(SiteKind::Fort, false, SiteKind::Bandit, 0, -1) ==
          SiteControl::Contested);
    CHECK(ReadSiteControl(SiteKind::Fort, false, SiteKind::Unknown, -2, 1) == SiteControl::Unknown);
    CHECK(ReadSiteControl(SiteKind::Fort, false, SiteKind::Bandit) == SiteControl::Unknown);
    CHECK(ReadSiteControl(SiteKind::Fort, false, SiteKind::Bandit, 17, 1) == SiteControl::Unknown);
    CHECK(ReadSiteControl(SiteKind::Fort, false, SiteKind::Bandit, 0, std::nan("")) ==
          SiteControl::Unknown);

    auto c = Fixture();
    Economy coalesced(c);
    auto quote = coalesced.Price("destination", "iron", 10, 4, true).total;
    coalesced.ObserveSite("camp", SiteControl::Hostile, .1);
    coalesced.ObserveSite("camp", SiteControl::Cleared, .2);
    coalesced.ObserveSite("camp", SiteControl::Hostile, .3);
    CHECK(coalesced.Get().security.pending.size() == 1);
    CHECK(coalesced.Get().security.pending.front().control == SiteControl::Hostile);
    CHECK(coalesced.Price("destination", "iron", 10, 4, true).total == quote);
    coalesced.Advance(1);
    CHECK(coalesced.SecurityConditions().routes.at("road").pressure > 0);
    auto broken = c;
    broken.security.sites.push_back(broken.security.sites.front());
    Rejects([&] { broken.Validate(); });
    broken = c;
    broken.security.sites.front().routes["missing"] = 1;
    Rejects([&] { broken.Validate(); });
    broken = c;
    broken.security.sites.front().routes["road"] = std::nan("");
    Rejects([&] { broken.Validate(); });

    Economy e(c), other(c);
    e.NewGame(0, 42);
    double initial = 0;
    for (const auto &[id, market] : e.Get().markets)
        for (const auto &[good, s] : market.goods)
            initial += s.quantity;
    CHECK(e.ObserveSite("camp", SiteControl::Hostile, .1));
    auto observed = e.Save();
    CHECK(!e.ObserveSite("camp", SiteControl::Hostile, .8));
    CHECK(e.Save() == observed);
    CHECK(!e.ObserveSite("crypt", SiteControl::Hostile, .8));
    CHECK(e.SecurityConditions().routes.at("road").pressure == 0);
    e.Advance(1);
    double pressure = e.SecurityConditions().routes.at("road").pressure;
    CHECK(pressure > 0 && pressure < .8);
    // A security change itself does not alter production or recipe coefficients.
    other.NewGame(0, 42);
    other.Advance(1);
    for (const auto &[market, data] : e.Get().markets)
        for (const auto &[good, s] : data.goods)
            CHECK(s.produced == other.Get().markets.at(market).goods.at(good).produced);

    e.Advance(15);
    auto oldShipments = e.Get().shipments;
    CHECK(!oldShipments.empty());
    CHECK(e.ObserveSite("camp", SiteControl::Cleared, 30.2));
    auto pending = e.Save();
    other.Load(pending);
    CHECK(other.Save() == pending);
    e.Advance(30, 32);
    CHECK(e.SecurityConditions().routes.at("road").pressure == pressure);
    other.Advance(30, 32);
    CHECK(e.Save() == other.Save());
    auto inFlight = e.Get().shipments;
    e.Advance(31);
    CHECK(e.SecurityConditions().routes.at("road").pressure == 0);
    for (const auto &s : inFlight)
        if (s.arrives > 31) {
            auto found = std::find_if(e.Get().shipments.begin(), e.Get().shipments.end(),
                                      [&](const auto &n) { return n.id == s.id; });
            CHECK(found != e.Get().shipments.end());
            CHECK(found->arrives == s.arrives && found->loss == s.loss &&
                  found->quantity == s.quantity);
        }
    while (e.Advance(120, 7)) {
    }
    while (other.Advance(120, 1)) {
    }
    CHECK(e.Save() == other.Save());
    Conservation(e, initial);
    auto settled = e.Save();
    CHECK(!e.ObserveSite("camp", SiteControl::Cleared, 120.2));
    CHECK(e.Save() == settled);
    Rejects([&] { e.ObserveSite("camp", SiteControl::Hostile, 119); });
    Rejects([&] { e.ObserveSite("absent", SiteControl::Hostile, 120); });
    Rejects([&] { e.ObserveSite("camp", static_cast<SiteControl>(77), 120); });
    CHECK(e.Save() == settled);
    CHECK(e.ObserveSite("camp", SiteControl::Hostile, 120.2));
    e.Advance(121);
    CHECK(e.SecurityConditions().routes.at("road").pressure == pressure);
    e.ObserveSite("fort", SiteControl::Imperial, 121.5);
    e.Advance(122);
    auto guards = e.SecurityConditions();
    CHECK(guards.routes.at("road").pressure < pressure);
    CHECK(guards.routes.at("road").riskScale < 1);
    CHECK(guards.demand.at("destination").at("food") == 3);
    other.Load(e.Save());
    other.ObserveSite("fort", SiteControl::Stormcloak, 122);
    other.Advance(123);
    e.Advance(123);
    CHECK(other.SecurityConditions().routes.at("road").pressure ==
          e.SecurityConditions().routes.at("road").pressure);
    CHECK(other.Get().markets.at("destination").goods.at("food").quantity ==
          e.Get().markets.at("destination").goods.at("food").quantity);

    auto valid = e.Save();
    auto bad = valid;
    bad.erase("security");
    Rejects([&] { e.Load(bad); });
    CHECK(e.Save() == valid);
    bad = valid;
    bad["security"]["sites"]["camp"] = "invalid";
    Rejects([&] { e.Load(bad); });
    CHECK(e.Save() == valid);
    bad = valid;
    bad["security"]["pending"] =
        json::array({{{"site", "camp"}, {"control", "cleared"}, {"day", 123}}});
    Rejects([&] { e.Load(bad); });
    CHECK(e.Save() == valid);
    bad["security"]["pending"][0]["day"] = 125;
    bad["security"]["pending"].push_back(bad["security"]["pending"][0]);
    Rejects([&] { e.Load(bad); });
    CHECK(e.Save() == valid);
    auto legacy = valid;
    legacy["schema"] = 3;
    legacy.erase("security");
    e.Load(legacy);
    CHECK(e.Get().security.sites.empty() && e.Get().security.pending.empty());
    auto restored = e.Save();
    restored["schema"] = 3;
    restored.erase("security");
    CHECK(restored == legacy);
    e.NewGame(5, 77);
    CHECK(e.Get().security.sites.empty() && e.Get().security.pending.empty());

    // Many added camps have a bounded combined effect. Unconfigured saved IDs
    // cannot affect a route, and disabling the feature restores baseline math.
    auto many = c;
    many.security.sites.clear();
    SecurityState states;
    for (int i = 0; i < 500; ++i) {
        auto site = c.security.sites.front();
        site.id = "camp" + std::to_string(i);
        many.security.sites.push_back(site);
        states.sites[site.id] = SiteControl::Hostile;
    }
    many.Validate();
    auto worst = EvaluateSecurity(many, states).routes.at("road");
    CHECK(worst.pressure <= .8 && worst.capacity >= .48 && worst.delay <= 1.48);
    states.sites["unconfigured"] = SiteControl::Hostile;
    CHECK(EvaluateSecurity(many, states).routes.at("road").pressure == worst.pressure);
    auto disabled = c;
    disabled.security.enabled = false;
    auto plain = disabled;
    plain.security = {};
    Economy off(disabled), baseline(plain);
    off.NewGame(0, 123);
    baseline.NewGame(0, 123);
    CHECK(!off.ObserveSite("camp", SiteControl::Hostile, 0));
    off.Advance(120, 120);
    baseline.Advance(120, 120);
    CHECK(off.Save() == baseline.Save());

    // Identical seeds and initial stocks: compare an unsafe road to a clear one,
    // then a clear fort to an occupied fort. These are capacity-limited fixtures.
    double unsafePrice = 0, safePrice = 0, emptyFood = 0, occupiedFood = 0;
    for (int seed = 1; seed <= 32; ++seed) {
        Economy unsafe(c), safe(c), empty(c), occupied(c);
        for (auto *sim : {&unsafe, &safe, &empty, &occupied})
            sim->NewGame(0, seed);
        unsafe.ObserveSite("camp", SiteControl::Hostile, 0);
        occupied.ObserveSite("fort", SiteControl::Imperial, 0);
        for (int day = 1; day <= 90; ++day) {
            for (auto *sim : {&unsafe, &safe, &empty, &occupied})
                sim->Advance(day);
            if (day > 30) {
                unsafePrice += unsafe.Multiplier("destination", "iron");
                safePrice += safe.Multiplier("destination", "iron");
                emptyFood += empty.Multiplier("destination", "food");
                occupiedFood += occupied.Multiplier("destination", "food");
            }
        }
        unsafe.Validate();
        safe.Validate();
        occupied.Validate();
        Conservation(unsafe, initial);
        Conservation(safe, initial);
        Conservation(occupied, initial);
    }
    CHECK(safePrice < unsafePrice);
    CHECK(occupiedFood > emptyFood);
    std::cout << "Fixture: safer-route iron price " << (safePrice / unsafePrice - 1) * 100
              << "%; garrison food price " << (occupiedFood / emptyFood - 1) * 100 << "%\n";
}
int main(int argc, char **argv) {
    try {
        CHECK(argc == 3);
        std::ifstream f(argv[1]), s(argv[2]);
        auto data = json::parse(f);
        data["security"] = json::parse(s);
        auto shipped = Config::Parse(data);
        CHECK(shipped.security.sites.size() == 10);
        CHECK(shipped.security.enabled);
        Run();
        std::cout
            << "PASS: classification, fort controls, bounded effects, production isolation, "
               "dated catch-up, in-flight shipments, persistence, migration and conservation\n";
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
