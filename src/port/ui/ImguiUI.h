#pragma once
#include <libultraship/libultraship.h>

namespace GameUI {
    void SetupGuiElements();
    void Destroy();
}

class GameMenuBar : public Ship::GuiMenuBar {
  public:
    using Ship::GuiMenuBar::GuiMenuBar;
    void SetVisibility(bool visible) override {
        Ship::GuiMenuBar::SetVisibility(visible);
    }
  protected:
    void DrawElement() override;
    void InitElement() override {};
    void UpdateElement() override {};

#ifdef __IOS__
    // Track which section is currently open on iOS
    int mOpenSection = -1;  // -1 = none, 0 = Starship, 1 = Settings, 2 = Enhancements, 3 = Cheats, 4 = Developer
#endif
};