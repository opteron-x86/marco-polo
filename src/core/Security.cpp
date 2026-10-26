#include "Security.h"
#include "Economy.h"
#include <algorithm>
#include <cmath>
#include <set>
#include <stdexcept>

namespace marco {
namespace {
void Require(bool value, const char *message) {
    if (!value)
        throw std::runtime_error(message);
}
bool Range(double n, double lo, double hi) {
    return std::isfinite(n) && n >= lo && n <= hi;
}
SiteKind Kind(const std::string &s) {
    if (s == "unknown")
        return SiteKind::Unknown;
    if (s == "bandit")
        return SiteKind::Bandit;
    if (s == "forsworn")
        return SiteKind::Forsworn;
    if (s == "crypt")
        return SiteKind::Crypt;
    if (s == "fort")
        return SiteKind::Fort;
    throw std::runtime_error("Unknown security site kind: " + s);
}
bool ValidControl(SiteControl c) {
    return c >= SiteControl::Unknown && c <= SiteControl::Contested;
}
} // namespace
std::string ControlName(SiteControl c) {
    switch (c) {
    case SiteControl::Unknown:
        return "unknown";
    case SiteControl::Hostile:
        return "hostile";
    case SiteControl::Cleared:
        return "cleared";
    case SiteControl::Imperial:
        return "imperial";
    case SiteControl::Stormcloak:
        return "stormcloak";
    case SiteControl::Contested:
        return "contested";
    }
    throw std::runtime_error("Invalid site control");
}
SiteKind ClassifySite(const std::vector<std::string> &keywords) {
    auto has = [&](const char *s) {
        return std::find(keywords.begin(), keywords.end(), s) != keywords.end();
    };
    if (has("CWFort") && has("LocTypeMilitaryFort"))
        return SiteKind::Fort;
    bool bandit = has("LocTypeBanditCamp"), forsworn = has("LocTypeForswornCamp"),
         crypt = has("LocTypeDraugrCrypt");
    if (static_cast<int>(bandit) + forsworn + crypt != 1)
        return SiteKind::Unknown;
    return bandit ? SiteKind::Bandit : forsworn ? SiteKind::Forsworn : SiteKind::Crypt;
}
SiteControl ReadSiteControl(SiteKind kind, bool cleared, SiteKind initial,
                            std::optional<double> fort, std::optional<double> owner) {
    if (kind == SiteKind::Unknown || kind == SiteKind::Crypt)
        return SiteControl::Unknown;
    if (kind == SiteKind::Fort) {
        if (!fort || !owner || !std::isfinite(*fort) || !std::isfinite(*owner))
            return SiteControl::Unknown;
        if (*fort == -2) {
            if (cleared)
                return SiteControl::Cleared;
            return initial == SiteKind::Bandit || initial == SiteKind::Forsworn
                       ? SiteControl::Hostile
                       : SiteControl::Unknown;
        }
        // CWResetGarrison makes the owner positive after changing enable markers.
        // Negative owners are an unfinished ownership transfer, not a garrison.
        if (*fort == 0) {
            if (*owner == 1)
                return SiteControl::Imperial;
            if (*owner == 2)
                return SiteControl::Stormcloak;
            if (*owner == -1 || *owner == -2)
                return SiteControl::Contested;
        }
        return SiteControl::Unknown;
    }
    return cleared ? SiteControl::Cleared : SiteControl::Hostile;
}
SecurityConfig SecurityConfig::Parse(const json &j) {
    Require(j.at("schema").get<int>() == 1, "Unsupported security configuration");
    SecurityConfig c;
    c.enabled = j.value("enabled", true);
    c.maximumPressure = j.value("maximumPressure", .8);
    c.maximumExtraDemand = j.value("maximumExtraDemand", .3);
    for (const auto &v : j.at("sites")) {
        SecuritySite s;
        s.id = v.at("location");
        s.name = v.at("name");
        s.market = v.at("market");
        s.kind = Kind(v.value("kind", "unknown"));
        s.initialOccupants = Kind(v.value("initialOccupants", "unknown"));
        s.threat = v.value("threat", 1.0);
        s.protection = v.value("protection", 1.0);
        s.routes = v.value("routes", std::map<std::string, double>{});
        s.demand = v.value("demand", std::map<std::string, double>{});
        c.sites.push_back(std::move(s));
    }
    return c;
}
void SecurityConfig::Validate(const Config &c) const {
    Require(sites.size() <= 512 && Range(maximumPressure, 0, .9) &&
                Range(maximumExtraDemand, 0, .5),
            "Invalid security policy");
    std::set<std::string> ids;
    for (const auto &s : sites) {
        Require(!s.id.empty() && s.id.size() <= 256 && ids.insert(s.id).second && !s.name.empty() &&
                    s.name.size() <= 200,
                "Invalid or duplicate security site");
        Require(s.kind >= SiteKind::Unknown && s.kind <= SiteKind::Fort &&
                    s.initialOccupants >= SiteKind::Unknown && s.initialOccupants <= SiteKind::Fort,
                "Invalid security site kind");
        c.Market(s.market);
        Require(Range(s.threat, 0, 5) && Range(s.protection, 0, 5), "Invalid site strength");
        for (const auto &[id, exposure] : s.routes) {
            Require(std::any_of(c.routes.begin(), c.routes.end(),
                                [&](const auto &r) { return r.id == id; }),
                    "Unknown security route");
            Require(Range(exposure, 0, 1), "Invalid route exposure");
        }
        for (const auto &[good, amount] : s.demand) {
            c.Good(good);
            Require(s.kind == SiteKind::Fort && Range(amount, 0, .5), "Invalid garrison demand");
        }
    }
}
TradeConditions EvaluateSecurity(const Config &c, const SecurityState &state) {
    TradeConditions out;
    if (!c.security.enabled)
        return out;
    std::map<std::string, double> threats, guards;
    for (const auto &s : c.security.sites) {
        if (s.kind == SiteKind::Unknown || s.kind == SiteKind::Crypt)
            continue;
        auto it = state.sites.find(s.id);
        if (it == state.sites.end())
            continue;
        bool military = s.kind == SiteKind::Fort && (it->second == SiteControl::Imperial ||
                                                     it->second == SiteControl::Stormcloak);
        for (const auto &[route, exposure] : s.routes) {
            if (it->second == SiteControl::Hostile || it->second == SiteControl::Contested)
                threats[route] += s.threat * exposure;
            if (military)
                guards[route] += s.protection * exposure;
        }
        if (military)
            for (const auto &[good, amount] : s.demand)
                out.demand[s.market][good] += amount;
    }
    for (const auto &r : c.routes) {
        auto &v = out.routes[r.id];
        v.protection = 1 - std::exp(-guards[r.id]);
        v.pressure =
            c.security.maximumPressure * (1 - std::exp(-threats[r.id] / (1 + guards[r.id])));
        v.capacity = 1 - .65 * v.pressure;
        v.delay = 1 + .6 * v.pressure;
        v.riskScale = 1 - .6 * v.protection;
        v.extraRisk = .18 * v.pressure;
        v.cost = (1 + .75 * v.pressure) * (1 - .25 * v.protection);
    }
    for (auto &[market, goods] : out.demand)
        for (auto &[good, amount] : goods) {
            const auto &demand = c.Market(market).demand;
            auto it = demand.find(good);
            amount = std::min(amount, c.security.maximumExtraDemand) *
                     (it == demand.end() ? 0 : it->second);
        }
    return out;
}
void ValidateSecurity(const SecurityState &s, double day) {
    Require(s.sites.size() <= 4096 && s.pending.size() <= 8192, "Oversized security state");
    for (const auto &[id, control] : s.sites)
        Require(!id.empty() && id.size() <= 256 && ValidControl(control), "Invalid saved site");
    std::set<std::pair<double, std::string>> seen;
    double previous = day;
    for (const auto &change : s.pending) {
        Require(!change.site.empty() && change.site.size() <= 256 && ValidControl(change.control) &&
                    std::isfinite(change.day) && change.day > day && change.day >= previous &&
                    std::floor(change.day) == change.day &&
                    seen.emplace(change.day, change.site).second,
                "Invalid pending site change");
        previous = change.day;
    }
}
void to_json(json &j, const SecurityState &s) {
    j = {{"sites", json::object()}, {"pending", json::array()}};
    for (const auto &[id, control] : s.sites)
        j["sites"][id] = ControlName(control);
    for (const auto &change : s.pending)
        j["pending"].push_back(
            {{"site", change.site}, {"control", ControlName(change.control)}, {"day", change.day}});
}
void from_json(const json &j, SecurityState &s) {
    auto control = [](const std::string &name) {
        for (auto c : {SiteControl::Unknown, SiteControl::Hostile, SiteControl::Cleared,
                       SiteControl::Imperial, SiteControl::Stormcloak, SiteControl::Contested})
            if (ControlName(c) == name)
                return c;
        throw std::runtime_error("Unknown saved site control");
    };
    Require(j.at("sites").is_object() && j.at("pending").is_array() &&
                j.at("sites").size() <= 4096 && j.at("pending").size() <= 8192,
            "Invalid security save dimensions");
    SecurityState incoming;
    for (const auto &[id, value] : j.at("sites").items())
        incoming.sites[id] = control(value.get<std::string>());
    for (const auto &v : j.at("pending"))
        incoming.pending.push_back({v.at("site"), control(v.at("control")), v.at("day")});
    s = std::move(incoming);
}
} // namespace marco
