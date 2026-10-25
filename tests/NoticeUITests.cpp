#include "runtime/NoticeUI.h"
#include <functional>
#include <iostream>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#define CHECK(x)                                                                                   \
    do {                                                                                           \
        if (!(x))                                                                                  \
            throw std::runtime_error(std::string(#x) + " at " + std::to_string(__LINE__));         \
    } while (false)

// Minimal Scaleform boundary: null/undefined/primitive receivers throw rather
// than crashing the test process, as GFxValue::GetMember does in release builds.
struct Value {
    struct Object;
    std::shared_ptr<Object> object;
    double number{};
    bool boolean{};
    Value() = default;
    explicit Value(double n) : number(n) {}
    explicit Value(bool b) : boolean(b) {}
    bool IsObject() const {
        return object != nullptr;
    }
    bool GetMember(const char *, Value *);
    bool Invoke(const char *, Value *, const Value * = nullptr, unsigned = 0);
    static Value MakeObject();
};
struct Value::Object {
    std::map<std::string, Value> members;
    std::map<std::string, std::function<void(Value *, const Value *, unsigned)>> methods;
    std::vector<std::string> calls;
};
Value Value::MakeObject() {
    Value v;
    v.object = std::make_shared<Object>();
    return v;
}
bool Value::GetMember(const char *key, Value *out) {
    CHECK(IsObject());
    auto found = object->members.find(key);
    if (found == object->members.end())
        return false;
    *out = found->second;
    return true; // A present member can still be null.
}
bool Value::Invoke(const char *method, Value *out, const Value *args, unsigned count) {
    CHECK(IsObject());
    auto found = object->methods.find(method);
    if (found == object->methods.end())
        return false;
    object->calls.emplace_back(method);
    found->second(out, args, count);
    return true;
}
void ReadButton() {
    using marco::runtime::ShowPostingReadButton;
    CHECK(!ShowPostingReadButton<Value>(nullptr));
    Value undefined;
    CHECK(!ShowPostingReadButton(&undefined));
    Value primitive(1.0);
    CHECK(!ShowPostingReadButton(&primitive));
    auto root = Value::MakeObject();
    CHECK(!ShowPostingReadButton(&root));
    root.object->members["navPanel"] = Value();
    CHECK(!ShowPostingReadButton(&root));
    root.object->members["navPanel"] = Value(true);
    CHECK(!ShowPostingReadButton(&root));
    auto panel = Value::MakeObject();
    root.object->members["navPanel"] = panel;
    CHECK(!ShowPostingReadButton(&root)); // Unknown UI without the button helper.
    root.object->methods["getEquipButtonData"] = [](Value *, const Value *, unsigned) {};
    CHECK(!ShowPostingReadButton(&root)); // Helper returns undefined during initialization.
    CHECK(panel.object->calls.empty());   // Keep original controls in both cases.

    auto translatedButton = Value::MakeObject();
    root.object->methods["getEquipButtonData"] = [&](Value *out, const Value *args,
                                                     unsigned count) {
        CHECK(count == 2 && args[0].number == 4.0 && !args[1].boolean);
        *out = translatedButton;
    };
    panel.object->methods["clearButtons"] = [](Value *, const Value *, unsigned count) {
        CHECK(count == 0);
    };
    panel.object->methods["addButton"] = [&](Value *, const Value *args, unsigned count) {
        CHECK(count == 1 && args[0].object == translatedButton.object);
    };
    panel.object->methods["updateButtons"] = [](Value *, const Value *args, unsigned count) {
        CHECK(count == 1 && args[0].boolean);
    };
    // Reproduce the crashing selection: itemCard exists, itemInfo is null.
    // The same code used by the plugin must work before, during and after
    // card initialization, without reading a stale previous item's type.
    auto card = Value::MakeObject();
    card.object->members["itemInfo"] = Value();
    for (auto state : {Value(), card}) {
        root.object->members["itemCard"] = state;
        panel.object->calls.clear();
        CHECK(ShowPostingReadButton(&root));
        CHECK((panel.object->calls ==
               std::vector<std::string>{"clearButtons", "addButton", "updateButtons"}));
    }
    auto staleInfo = Value::MakeObject();
    staleInfo.object->members["type"] = Value(1.0); // Previous selection was a weapon.
    card.object->members["itemInfo"] = staleInfo;
    CHECK(ShowPostingReadButton(&root));
}
int main() {
    try {
        ReadButton();
        std::cout << "PASS: posting controls with absent/null/initializing/stale item cards\n";
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
