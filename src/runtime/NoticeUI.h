#pragma once

namespace marco::runtime {
// SkyUI's Inventory.ICT_BOOK is part of the SWF protocol, not a Skyrim
// executable offset. The native selection already identifies our BOOK. Its
// item card may still be null while updateBottomBar runs during selection.
template <class Value> bool ShowPostingReadButton(Value *root) {
    if (!root || !root->IsObject())
        return false;
    Value panel, button;
    if (!root->GetMember("navPanel", &panel) || !panel.IsObject())
        return false;
    Value parameters[]{Value(4.0), Value(false)};
    if (!root->Invoke("getEquipButtonData", &button, parameters, 2) || !button.IsObject())
        return false;
    // Preserve the menu's translated label and input bindings.
    if (!panel.Invoke("clearButtons", nullptr) || !panel.Invoke("addButton", nullptr, &button, 1))
        return false;
    Value refresh(true);
    return panel.Invoke("updateButtons", nullptr, &refresh, 1);
}
} // namespace marco::runtime
