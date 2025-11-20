#include "SDLButtonToButtonMapping.h"
#include <spdlog/spdlog.h>
#include "utils/StringHelper.h"
#include "window/gui/IconsFontAwesome4.h"
#include "public/bridge/consolevariablebridge.h"
#include "controller/controldeck/ControlDeck.h"
#include "Context.h"

// iOS button state cache for bypassing SDL3's broken virtual joystick queries
#ifdef __IOS__
extern "C" bool iOS_GetButtonState(int button);
#endif

namespace Ship {
SDLButtonToButtonMapping::SDLButtonToButtonMapping(uint8_t portIndex, CONTROLLERBUTTONS_T bitmask,
                                                   int32_t sdlControllerButton)
    : ControllerInputMapping(PhysicalDeviceType::SDLGamepad), SDLButtonToAnyMapping(sdlControllerButton),
      ControllerButtonMapping(PhysicalDeviceType::SDLGamepad, portIndex, bitmask) {
}

// Helper function to map gamepad button enums to joystick button indices
// CRITICAL FIX: This MUST match the mapping string in MobileImpl.cpp: a:b0,b:b1,x:b2,y:b3,back:b4,guide:b5,start:b6
static int GamepadButtonToJoystickButton(SDL_GamepadButton button) {
    switch (button) {
        case SDL_GAMEPAD_BUTTON_SOUTH:         return 0;   // A button (a:b0)
        case SDL_GAMEPAD_BUTTON_EAST:          return 1;   // B button (b:b1)
        case SDL_GAMEPAD_BUTTON_WEST:          return 2;   // X button (x:b2)
        case SDL_GAMEPAD_BUTTON_NORTH:         return 3;   // Y button (y:b3) - BOOST
        case SDL_GAMEPAD_BUTTON_BACK:          return 4;   // SELECT/BACK button (back:b4)
        case SDL_GAMEPAD_BUTTON_GUIDE:         return 5;   // GUIDE button (guide:b5)
        case SDL_GAMEPAD_BUTTON_START:         return 6;   // START button (start:b6)
        case SDL_GAMEPAD_BUTTON_LEFT_STICK:    return 7;   // Left stick button (leftstick:b7)
        case SDL_GAMEPAD_BUTTON_RIGHT_STICK:   return 8;   // Right stick button (rightstick:b8)
        case SDL_GAMEPAD_BUTTON_LEFT_SHOULDER: return 9;   // L button (leftshoulder:b9)
        case SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER:return 10;  // R button (rightshoulder:b10)
        case SDL_GAMEPAD_BUTTON_DPAD_UP:       return 11;  // D-pad Up (dpup:b11)
        case SDL_GAMEPAD_BUTTON_DPAD_DOWN:     return 12;  // D-pad Down (dpdown:b12)
        case SDL_GAMEPAD_BUTTON_DPAD_LEFT:     return 13;  // D-pad Left (dpleft:b13)
        case SDL_GAMEPAD_BUTTON_DPAD_RIGHT:    return 14;  // D-pad Right (dpright:b14)
        default:                                return -1;  // Unknown button
    }
}

void SDLButtonToButtonMapping::UpdatePad(CONTROLLERBUTTONS_T& padButtons) {
    // Only log for A button (button 0) to avoid spam
    static bool hasLoggedGamepads = false;
    static int logCounter = 0;
    bool isAButton = (mControllerButton == SDL_GAMEPAD_BUTTON_SOUTH);

    if (Context::GetInstance()->GetControlDeck()->GamepadGameInputBlocked()) {
        return;
    }

    auto connectedGamepads = Context::GetInstance()->GetControlDeck()->GetConnectedPhysicalDeviceManager()->GetConnectedSDLGamepadsForPort(mPortIndex);

    if (isAButton && !hasLoggedGamepads) {
        SPDLOG_INFO("[Game Engine] UpdatePad called for A button (SOUTH), port={}, found {} gamepads",
                    mPortIndex, connectedGamepads.size());
        hasLoggedGamepads = true;
    }

    for (const auto& [instanceId, gamepad] : connectedGamepads) {
        bool buttonState = false;

#ifdef __IOS__
        // CRITICAL FIX: On iOS, bypass SDL3's broken virtual joystick queries entirely
        // SDL3's virtual joystick state queries don't work - they always return false even after
        // successful SDL_SetJoystickVirtualButton calls. Use direct button state cache instead.
        int joystickButton = GamepadButtonToJoystickButton(mControllerButton);
        if (joystickButton >= 0) {
            buttonState = iOS_GetButtonState(joystickButton);
            if (isAButton && logCounter < 100) {
                SPDLOG_INFO("[Game Engine] iOS button state cache: button {} = {} (bypassing SDL queries)",
                            joystickButton, buttonState);
                logCounter++;
            }
        }
#else
        buttonState = SDL_GetGamepadButton(gamepad, mControllerButton);

        // Workaround for SDL3 virtual joystick: gamepad mapping doesn't translate virtual button states
        // Fall back to querying the joystick directly if gamepad returns false
        if (!buttonState) {
            SDL_Joystick* joystick = SDL_GetGamepadJoystick(gamepad);
            if (joystick) {
                int joystickButton = GamepadButtonToJoystickButton(mControllerButton);
                if (joystickButton >= 0) {
                    bool joystickButtonState = SDL_GetJoystickButton(joystick, joystickButton);
                    if (joystickButtonState) {
                        buttonState = true;
                        if (isAButton && logCounter < 5) {
                            SPDLOG_INFO("[Game Engine] Gamepad returned false, but joystick button {} is TRUE! Using joystick state.", joystickButton);
                        }
                    }
                }
            }
        }

        // Log every SDL_GetGamepadButton call for A button for first 100 calls
        if (isAButton && logCounter < 100) {
            SPDLOG_INFO("[Game Engine] SDL_GetGamepadButton(gamepad={}, SOUTH) returned: {} (instanceId={})",
                        (void*)gamepad, buttonState, instanceId);
            logCounter++;
        }
#endif

        if (buttonState) {
            padButtons |= mBitmask;
            return;
        }
    }
}

int8_t SDLButtonToButtonMapping::GetMappingType() {
    return MAPPING_TYPE_GAMEPAD;
}

std::string SDLButtonToButtonMapping::GetButtonMappingId() {
    return StringHelper::Sprintf("P%d-B%d-SDLB%d", mPortIndex, mBitmask, mControllerButton);
}

void SDLButtonToButtonMapping::SaveToConfig() {
    const std::string mappingCvarKey = CVAR_PREFIX_CONTROLLERS ".ButtonMappings." + GetButtonMappingId();
    CVarSetString(StringHelper::Sprintf("%s.ButtonMappingClass", mappingCvarKey.c_str()).c_str(),
                  "SDLButtonToButtonMapping");
    CVarSetInteger(StringHelper::Sprintf("%s.Bitmask", mappingCvarKey.c_str()).c_str(), mBitmask);
    CVarSetInteger(StringHelper::Sprintf("%s.SDLControllerButton", mappingCvarKey.c_str()).c_str(), mControllerButton);
    CVarSave();
}

void SDLButtonToButtonMapping::EraseFromConfig() {
    const std::string mappingCvarKey = CVAR_PREFIX_CONTROLLERS ".ButtonMappings." + GetButtonMappingId();

    CVarClear(StringHelper::Sprintf("%s.ButtonMappingClass", mappingCvarKey.c_str()).c_str());
    CVarClear(StringHelper::Sprintf("%s.Bitmask", mappingCvarKey.c_str()).c_str());
    CVarClear(StringHelper::Sprintf("%s.SDLControllerButton", mappingCvarKey.c_str()).c_str());
    CVarSave();
}

std::string SDLButtonToButtonMapping::GetPhysicalDeviceName() {
    return SDLButtonToAnyMapping::GetPhysicalDeviceName();
}

std::string SDLButtonToButtonMapping::GetPhysicalInputName() {
    return SDLButtonToAnyMapping::GetPhysicalInputName();
}
} // namespace Ship
