#include "Runtime.h"
#include <spdlog/sinks/basic_file_sink.h>

namespace {
void Message(SKSE::MessagingInterface::Message *m) {
    if (!m)
        return;
    using namespace marco::runtime;
    switch (m->type) {
    case SKSE::MessagingInterface::kDataLoaded:
        if (Initialize()) {
            InstallBarter();
            InstallMissives();
            RegisterMenu();
        }
        break;
    case SKSE::MessagingInterface::kPreLoadGame:
        Reset();
        break;
    case SKSE::MessagingInterface::kPostLoadGame:
        if (m->data)
            Start();
        break;
    case SKSE::MessagingInterface::kNewGame:
        Reset();
        Start();
        break;
    default:
        break;
    }
}
} // namespace
extern "C" __declspec(dllexport) constinit auto SKSEPlugin_Version = []() noexcept {
    SKSE::PluginVersionData d{};
    d.PluginName("MarcoPolo");
    d.AuthorName("opteron-x86");
    d.PluginVersion(REL::Version(MARCO_VERSION_MAJOR, MARCO_VERSION_MINOR, MARCO_VERSION_PATCH, 0));
    d.UsesAddressLibrary();
    d.UsesUpdatedStructs();
    d.versionIndependenceEx |= SKSE::PluginVersionData::kVersionIndependentEx_AddressLibraryV5;
    d.CompatibleVersions({REL::Version(1, 6, 1170, 0)});
    return d;
}();
extern "C" __declspec(dllexport) bool SKSEPlugin_Load(const SKSE::LoadInterface *skse) {
    if (!skse || skse->IsEditor())
        return false;
    try {
        auto p = SKSE::log::log_directory();
        if (!p)
            return false;
        *p /= "MarcoPolo.log";
        auto sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(p->string(), true);
        auto logger = std::make_shared<spdlog::logger>("global", sink);
        logger->set_level(spdlog::level::info);
        logger->flush_on(spdlog::level::info);
        spdlog::set_default_logger(logger);
        SKSE::InitInfo init;
        init.log = false;
        SKSE::Init(skse, init);
        SKSE::log::info("Marco Polo {} | Skyrim {}", MARCO_VERSION,
                        skse->RuntimeVersion().string());
        // Runtime-dependent barter ABI is deliberately gated to the tested target.
        if (skse->RuntimeVersion() != REL::Version(1, 6, 1170, 0)) {
            SKSE::log::error("This build targets Skyrim 1.6.1170. No hooks installed.");
            return false;
        }
        auto *s = SKSE::GetSerializationInterface();
        if (!s || !SKSE::GetTaskInterface())
            return false;
        s->SetUniqueID(0x4D504F4C);
        s->SetSaveCallback(marco::runtime::Save);
        s->SetLoadCallback(marco::runtime::Load);
        s->SetRevertCallback([](auto *) { marco::runtime::Reset(); });
        return SKSE::GetMessagingInterface()->RegisterListener(Message);
    } catch (const std::exception &e) {
        SKSE::log::error("Load failed: {}", e.what());
        return false;
    }
}
