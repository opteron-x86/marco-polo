#pragma once
#include <map>
#include <nlohmann/json.hpp>
#include <optional>
#include <string>
#include <vector>

namespace marco {
struct Config;
enum class SiteKind { Unknown, Bandit, Forsworn, Crypt, Fort };
enum class SiteControl { Unknown, Hostile, Cleared, Imperial, Stormcloak, Contested };
std::string ControlName(SiteControl);
SiteKind ClassifySite(const std::vector<std::string> &keywords);
// Fort ownership alone is insufficient: CW initially gives nonmilitary forts
// an owner while CWFort == -2 keeps their military garrison out of the war.
SiteControl ReadSiteControl(SiteKind, bool cleared, SiteKind initialOccupants,
                            std::optional<double> fortStatus = {},
                            std::optional<double> owner = {});
struct SecuritySite {
    std::string id, name, market;
    SiteKind kind{SiteKind::Unknown}, initialOccupants{SiteKind::Unknown};
    double threat{1}, protection{1};
    std::map<std::string, double> routes, demand;
};
struct SecurityConfig {
    bool enabled{false};
    double maximumPressure{0.8}, maximumExtraDemand{0.3};
    std::vector<SecuritySite> sites;
    static SecurityConfig Parse(const nlohmann::json &);
    void Validate(const Config &) const;
};
struct SiteChange {
    std::string site;
    SiteControl control{SiteControl::Unknown};
    double day{};
};
struct SecurityState {
    std::map<std::string, SiteControl> sites;
    std::vector<SiteChange> pending;
};
struct RouteSecurity {
    double pressure{}, protection{};
    double capacity{1}, delay{1}, riskScale{1}, extraRisk{}, cost{1};
};
struct TradeConditions {
    std::map<std::string, RouteSecurity> routes;
    std::map<std::string, std::map<std::string, double>> demand;
};
TradeConditions EvaluateSecurity(const Config &, const SecurityState &);
void ValidateSecurity(const SecurityState &, double day);
void to_json(nlohmann::json &, const SecurityState &);
void from_json(const nlohmann::json &, SecurityState &);
} // namespace marco
