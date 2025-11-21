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
    mIsVisible = visible;
#if defined(__IOS__)
    // On iOS, control touch controls overlay based on menu visibility
    iOS_SetMenuOpen(visible);
#endif
    SyncVisibilityConsoleVariable();
}
} // namespace Ship
