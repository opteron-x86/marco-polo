#include "Runtime.h"
#include <cmath>
#include <filesystem>
#include <fstream>

namespace marco::runtime {
namespace {
constexpr std::string_view MenuName = "MarcoPoloTradeLedger";
constexpr char MoviePath[] = "MarcoPoloTradeLedger";
constexpr char MovieFile[] = "Interface/MarcoPoloTradeLedger.swf";
std::mutex preferencesMutex, commandsMutex;
LedgerPreferences preferences;
std::vector<std::string> commands;
std::atomic_uint hotkey{0x0D}, hotkeyModifier{0x36};
std::atomic_bool open{}, togglePending{};

void Close() {
    if (auto queue = RE::UIMessageQueue::GetSingleton())
        queue->AddMessage(MenuName, RE::UI_MESSAGE_TYPE::kHide, nullptr);
}
void Command(std::string key) {
    std::lock_guard lock(commandsMutex);
    if (commands.size() < 32)
        commands.push_back(std::move(key));
}
std::filesystem::path PreferencesPath() {
    return std::filesystem::path(DataPath) / "ledger.json";
}
// Caller holds preferencesMutex. A failed write leaves the previous file intact.
void SavePreferences() {
    auto target = PreferencesPath(), temp = target;
    temp += ".tmp";
    std::filesystem::create_directories(target.parent_path());
    {
        std::ofstream file(temp, std::ios::binary | std::ios::trunc);
        file << preferences.Save().dump(2) << '\n';
        file.close();
        if (!file)
            throw std::runtime_error("Cannot write ledger preferences");
    }
    if (!MoveFileExW(temp.c_str(), target.c_str(),
                     MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
        throw std::runtime_error(
            std::format("Cannot replace ledger preferences ({})", GetLastError()));
}
void LoadPreferences() {
    try {
        std::ifstream file(PreferencesPath());
        if (file)
            preferences = LedgerPreferences::Parse(json::parse(file));
    } catch (const std::exception &e) {
        SKSE::log::warn("Ledger preferences: {}; using defaults", e.what());
    }
    hotkey = preferences.hotkey;
    hotkeyModifier = preferences.hotkeyModifier;
}
RE::GFxValue ToFlash(RE::GFxMovie &movie, const json &j) {
    RE::GFxValue value;
    if (j.is_object()) {
        movie.CreateObject(&value);
        for (auto &[key, child] : j.items())
            value.SetMember(key.c_str(), ToFlash(movie, child));
    } else if (j.is_array()) {
        movie.CreateArray(&value);
        for (auto &child : j)
            value.PushBack(ToFlash(movie, child));
    } else if (j.is_string()) {
        movie.CreateString(&value, j.get_ref<const std::string &>().c_str());
    } else if (j.is_boolean()) {
        value.SetBoolean(j.get<bool>());
    } else if (j.is_number()) {
        value.SetNumber(j.get<double>());
    }
    return value;
}
class Bridge final : public RE::GFxFunctionHandler {
  public:
    explicit Bridge(std::uint64_t gen) : gen_(gen) {}
    void Call(Params &p) override {
        // The movie owns this handler. It never holds a pointer back to a menu or game form.
        if (gen_ != generation || p.argCount < 1 || !p.args[0].IsString())
            return;
        try {
            std::string_view action(p.args[0].GetString());
            if (action == "close") {
                Close();
                return;
            }
            if (p.argCount < 2)
                return;
            auto &value = p.args[1];
            if (action == "ready") {
                RE::GFxValue width, height, measured;
                if (value.IsObject() && value.GetMember("stageWidth", &width) && width.IsNumber() &&
                    value.GetMember("stageHeight", &height) && height.IsNumber() &&
                    value.GetMember("titleWidth", &measured) && measured.IsNumber())
                    SKSE::log::info("Trade ledger ready: {}x{}, game font aliases, title width {}",
                                    width.GetNumber(), height.GetNumber(), measured.GetNumber());
                return;
            }
            std::lock_guard lock(preferencesMutex);
            if (action == "preferences" && value.IsObject()) {
                auto updated = preferences.Save();
                for (const char *name : {"x", "y", "scale"}) {
                    RE::GFxValue member;
                    if (value.GetMember(name, &member) && member.IsNumber() &&
                        std::isfinite(member.GetNumber()))
                        updated[name] = member.GetNumber();
                }
                RE::GFxValue locked;
                if (value.GetMember("locked", &locked) && locked.IsBool())
                    updated["locked"] = locked.GetBool();
                preferences = LedgerPreferences::Parse(updated);
                SavePreferences();
            } else if (action == "pin" && value.IsString()) {
                std::string key(value.GetString());
                if (key.empty() || key.size() > 192)
                    return;
                if (!preferences.pins.erase(key) && preferences.pins.size() < 64)
                    preferences.pins.insert(std::move(key));
                SavePreferences();
            }
        } catch (const std::exception &e) {
            SKSE::log::error("Trade ledger callback: {}", e.what());
        }
    }

  private:
    std::uint64_t gen_;
};

class LedgerMenu final : public RE::IMenu {
  public:
    LedgerMenu() : gen_(generation.load()) {
        depthPriority = 3;
        menuFlags.set(Flag::kPausesGame, Flag::kUsesCursor, Flag::kUsesMenuContext, Flag::kModal,
                      Flag::kDisablePauseMenu, Flag::kRequiresUpdate, Flag::kUpdateUsesCursor);
        inputContext = Context::kMenuMode;
        try {
            auto view = View();
            auto manager = RE::BSScaleformManager::GetSingleton();
            if (!active || !view.ready)
                throw std::runtime_error("Market reports are not ready");
            if (!manager || !manager->loader)
                throw std::runtime_error("Skyrim's Scaleform loader is unavailable");
            std::error_code fileError;
            bool looseFile =
                std::filesystem::exists(std::filesystem::path("Data") / MovieFile, fileError);
            bool resourceFile = RE::BSScaleformManager::FileExists(MovieFile);
            SKSE::log::info("Trade ledger file {}: resource={}, loose={}, file status={}",
                            MovieFile, resourceFile, looseFile, fileError.value());
            if (!resourceFile)
                throw std::runtime_error(std::format("Skyrim cannot find {}", MovieFile));
            if (!manager->LoadMovie(this, uiMovie, MoviePath,
                                    RE::GFxMovieView::ScaleModeType::kNoScale) ||
                !uiMovie)
                throw std::runtime_error(
                    std::format("Scaleform could not create a movie from {}", MovieFile));
            // The standard loader supplies the active FontLib/fontconfig, including font mods.
            uiMovie->SetMouseCursorCount(1);
            auto bridge = RE::make_gptr<Bridge>(gen_);
            RE::GFxValue callback;
            uiMovie->CreateFunction(&callback, bridge.get());
            if (!uiMovie->SetVariable("_root.STEMNative", callback))
                throw std::runtime_error("Cannot attach ledger callback");
            LedgerPreferences copy;
            {
                std::lock_guard lock(preferencesMutex);
                copy = preferences;
            }
            auto payload = ToFlash(*uiMovie, LedgerPayload(view, copy));
            if (!uiMovie->Invoke("_root.SetData", nullptr, &payload, 1))
                throw std::runtime_error("Ledger movie has no SetData function");
            failed_ = false;
        } catch (const std::exception &e) {
            SKSE::log::error("Trade ledger: {}", e.what());
        }
    }
    ~LedgerMenu() override {
        open = false;
        std::lock_guard lock(commandsMutex);
        commands.clear();
    }
    void PostCreate() override {
        open = true;
        togglePending = false;
        if (failed_)
            Close();
    }
    RE::UI_MESSAGE_RESULTS ProcessMessage(RE::UIMessage &message) override {
        if (message.type == RE::UI_MESSAGE_TYPE::kHide ||
            message.type == RE::UI_MESSAGE_TYPE::kForceHide) {
            open = false;
            return RE::UI_MESSAGE_RESULTS::kHandled;
        }
        return RE::IMenu::ProcessMessage(message);
    }
    void AdvanceMovie(float interval, std::uint32_t) override {
        if (failed_ || !active || gen_ != generation) {
            if (!closing_) {
                closing_ = true;
                Close();
            }
            return;
        }
        std::vector<std::string> pending;
        {
            std::lock_guard lock(commandsMutex);
            pending.swap(commands);
        }
        for (auto &key : pending) {
            RE::GFxValue arg;
            uiMovie->CreateString(&arg, key.c_str());
            uiMovie->Invoke("_root.HandleKey", nullptr, &arg, 1);
        }
        uiMovie->Advance(interval);
    }

  private:
    std::uint64_t gen_;
    bool failed_{true}, closing_{};
};

bool CanOpen(RE::UI &ui) {
    if (!active || !View().ready || ui.GameIsPaused())
        return false;
    for (auto menu :
         {RE::MainMenu::MENU_NAME, RE::LoadingMenu::MENU_NAME, RE::Console::MENU_NAME,
          RE::DialogueMenu::MENU_NAME, RE::BarterMenu::MENU_NAME, RE::InventoryMenu::MENU_NAME,
          RE::ContainerMenu::MENU_NAME, RE::BookMenu::MENU_NAME, RE::MessageBoxMenu::MENU_NAME,
          RE::CraftingMenu::MENU_NAME, RE::MapMenu::MENU_NAME})
        if (ui.IsMenuOpen(menu))
            return false;
    return true;
}
void Toggle() {
    if (togglePending.exchange(true))
        return;
    auto gen = generation.load();
    SKSE::GetTaskInterface()->AddTask([gen] {
        togglePending = false;
        if (gen != generation)
            return;
        auto ui = RE::UI::GetSingleton();
        auto queue = RE::UIMessageQueue::GetSingleton();
        if (!ui || !queue)
            return;
        if (ui->IsMenuOpen(MenuName))
            Close();
        else if (CanOpen(*ui)) {
            std::lock_guard lock(commandsMutex);
            commands.clear();
            queue->AddMessage(MenuName, RE::UI_MESSAGE_TYPE::kShow, nullptr);
        }
    });
}
class Input final : public RE::BSTEventSink<RE::InputEvent *> {
  public:
    RE::BSEventNotifyControl ProcessEvent(RE::InputEvent *const *events,
                                          RE::BSTEventSource<RE::InputEvent *> *) override {
        if (!events)
            return RE::BSEventNotifyControl::kContinue;
        for (auto e = *events; e; e = e->next) {
            auto button = e->AsButtonEvent();
            if (!button || !button->IsDown())
                continue;
            auto key = button->GetIDCode();
            if (button->GetDevice() == RE::INPUT_DEVICE::kKeyboard) {
                if (hotkey != 0 && key == hotkey) {
                    auto modifier = hotkeyModifier.load();
                    auto manager = RE::BSInputDeviceManager::GetSingleton();
                    auto keyboard = manager ? manager->GetKeyboard() : nullptr;
                    bool held = false;
                    if (keyboard) {
                        // Current device state avoids latching a modifier across focus changes.
                        const auto &keys = keyboard->GetRuntimeData().curState;
                        held = modifier < std::size(keys) && (keys[modifier] & 0x80) != 0;
                    }
                    if (modifier == 0 || held)
                        Toggle();
                }
            } else if (open && button->GetDevice() == RE::INPUT_DEVICE::kGamepad) {
                switch (key) {
                case 0x0001:
                    Command("up");
                    break;
                case 0x0002:
                    Command("down");
                    break;
                case 0x0004:
                    Command("left");
                    break;
                case 0x0008:
                    Command("right");
                    break;
                case 0x0100:
                    Command("prevTab");
                    break;
                case 0x0200:
                    Command("nextTab");
                    break;
                case 0x2000:
                    Command("close");
                    break;
                default:
                    break;
                }
            }
        }
        return RE::BSEventNotifyControl::kContinue;
    }
};
} // namespace
void RegisterLedger() {
    auto ui = RE::UI::GetSingleton();
    auto input = RE::BSInputDeviceManager::GetSingleton();
    if (!ui || !input) {
        SKSE::log::error("Cannot register trade ledger: UI/input unavailable");
        return;
    }
    LoadPreferences();
    ui->Register(MenuName, []() -> RE::IMenu * { return new LedgerMenu; });
    static Input sink;
    input->AddEventSink(&sink);
    SKSE::log::info("Trade ledger registered; keyboard shortcut {}, modifier {}", hotkey.load(),
                    hotkeyModifier.load());
}
void ResetLedger() {
    togglePending = false;
    {
        std::lock_guard lock(commandsMutex);
        commands.clear();
    }
    if (open)
        Close();
}
} // namespace marco::runtime
