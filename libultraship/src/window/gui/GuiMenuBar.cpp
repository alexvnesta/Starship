#include "window/gui/GuiMenuBar.h"
#include "libultraship/libultraship.h"

#if defined(__IOS__)
extern "C" void iOS_SetMenuOpen(bool menuOpen);
#endif

namespace Ship {
GuiMenuBar::GuiMenuBar(const std::string& visibilityConsoleVariable, bool isVisible)
    : GuiElement(isVisible), mVisibilityConsoleVariable(visibilityConsoleVariable) {
    if (!mVisibilityConsoleVariable.empty()) {
        mIsVisible = CVarGetInteger(mVisibilityConsoleVariable.c_str(), mIsVisible);
        SyncVisibilityConsoleVariable();
#if defined(__IOS__)
        // Sync iOS touch controls state with menu visibility on startup
        SPDLOG_INFO("[GuiMenuBar] Constructor calling iOS_SetMenuOpen with mIsVisible={}", mIsVisible);
        iOS_SetMenuOpen(mIsVisible);
#endif
    }
}

GuiMenuBar::GuiMenuBar(const std::string& visibilityConsoleVariable) : GuiMenuBar(visibilityConsoleVariable, false) {
}

void GuiMenuBar::Draw() {
    if (!IsVisible()) {
        return;
    }

    DrawElement();
    // Sync up the IsVisible flag if it was changed by ImGui
    SyncVisibilityConsoleVariable();
}

void GuiMenuBar::SyncVisibilityConsoleVariable() {
    if (mVisibilityConsoleVariable.empty()) {
        return;
    }

    bool shouldSave = CVarGetInteger(mVisibilityConsoleVariable.c_str(), 0) != IsVisible();

    if (IsVisible()) {
        CVarSetInteger(mVisibilityConsoleVariable.c_str(), IsVisible());
    } else {
        CVarClear(mVisibilityConsoleVariable.c_str());
    }

    if (shouldSave) {
        Context::GetInstance()->GetWindow()->GetGui()->SaveConsoleVariablesNextFrame();
    }
}

void GuiMenuBar::SetVisibility(bool visible) {
    SPDLOG_INFO("[GuiMenuBar] SetVisibility called with visible={}", visible);
    mIsVisible = visible;
#if defined(__IOS__)
    // On iOS, control touch controls overlay based on menu visibility
    SPDLOG_INFO("[GuiMenuBar] SetVisibility calling iOS_SetMenuOpen with visible={}", visible);
    iOS_SetMenuOpen(visible);
#endif
    SyncVisibilityConsoleVariable();
}
} // namespace Ship
