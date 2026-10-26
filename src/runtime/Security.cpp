#include "Runtime.h"
#include <vector>

namespace marco::runtime {
namespace {
struct Site {
    const SecuritySite *definition{};
    RE::BGSLocation *location{};
};
std::vector<Site> sites;
std::optional<double> KeywordData(const RE::BGSLocation &location, const char *name) {
    for (const auto &value : location.keywordData)
        if (value.keyword && value.keyword->formEditorID == name)
            return value.data;
    return {};
}
} // namespace
void InitializeWorldTrade() {
    sites.clear();
    if (!economy->Definitions().security.enabled)
        return;
    for (const auto &definition : economy->Definitions().security.sites) {
        auto *form = Resolve(definition.id);
        auto *location = form ? form->As<RE::BGSLocation>() : nullptr;
        if (location) {
            std::vector<std::string> keywords;
            for (auto name : {"CWFort", "LocTypeMilitaryFort", "LocTypeBanditCamp",
                              "LocTypeForswornCamp", "LocTypeDraugrCrypt"})
                if (location->HasKeywordString(name))
                    keywords.emplace_back(name);
            if (ClassifySite(keywords) != definition.kind) {
                SKSE::log::warn("Route security: {} has unexpected location tags; inactive",
                                definition.name);
                location = nullptr;
            }
        } else {
            SKSE::log::warn("Route security: {} is unavailable; inactive", definition.id);
        }
        sites.push_back({&definition, location});
    }
    SKSE::log::info("Route security: {} configured sites", sites.size());
}
void RefreshWorldTrade(double now) {
    // Main-thread reads of a bounded, configured set of location records.
    // No reference scans, NPC spawning, keyword writes or quest changes.
    for (const auto &site : sites) {
        auto control = SiteControl::Unknown;
        if (auto *location = site.location)
            control = ReadSiteControl(
                site.definition->kind, location->IsCleared(), site.definition->initialOccupants,
                KeywordData(*location, "CWFort"), KeywordData(*location, "CWOwner"));
        if (economy->ObserveSite(site.definition->id, control, now) && debugLogging)
            SKSE::log::info("Route security: {} -> {}, observed day {} (future ticks only)",
                            site.definition->name, ControlName(control), now);
    }
}
} // namespace marco::runtime
