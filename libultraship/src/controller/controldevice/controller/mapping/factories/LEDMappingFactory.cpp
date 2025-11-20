#include "LEDMappingFactory.h"
#include "controller/controldevice/controller/mapping/sdl/SDLLEDMapping.h"
#include "public/bridge/consolevariablebridge.h"
#include "utils/StringHelper.h"
#include "libultraship/libultra/controller.h"
#include "controller/controldeck/ControlDeck.h"
#include "Context.h"

namespace Ship {
std::shared_ptr<ControllerLEDMapping> LEDMappingFactory::CreateLEDMappingFromConfig(uint8_t portIndex, std::string id) {
    const std::string mappingCvarKey = CVAR_PREFIX_CONTROLLERS ".LEDMappings." + id;
    const std::string mappingClass =
        CVarGetString(StringHelper::Sprintf("%s.LEDMappingClass", mappingCvarKey.c_str()).c_str(), "");

    int32_t colorSource = CVarGetInteger(StringHelper::Sprintf("%s.ColorSource", mappingCvarKey.c_str()).c_str(), -1);
    Color_RGB8 savedColor =
        CVarGetColor24(StringHelper::Sprintf("%s.SavedColor", mappingCvarKey.c_str()).c_str(), { 0, 0, 0 });

    if (colorSource != LED_COLOR_SOURCE_OFF && colorSource != LED_COLOR_SOURCE_SET &&
        colorSource != LED_COLOR_SOURCE_GAME) {
        // something about this mapping is invalid
        CVarClear(mappingCvarKey.c_str());
        CVarSave();
        return nullptr;
    }

    if (mappingClass == "SDLLEDMapping") {
        return std::make_shared<SDLLEDMapping>(portIndex, colorSource, savedColor);
    }

    return nullptr;
}

std::shared_ptr<ControllerLEDMapping> LEDMappingFactory::CreateLEDMappingFromSDLInput(uint8_t portIndex) {
    std::shared_ptr<ControllerLEDMapping> mapping = nullptr;

    for (auto [instanceId, gamepad] :
         Context::GetInstance()->GetControlDeck()->GetConnectedPhysicalDeviceManager()->GetConnectedSDLGamepadsForPort(
             portIndex)) {
        // SDL3: Check for LED capability using properties
        auto props = SDL_GetGamepadProperties(gamepad);
        bool hasRGBLED = SDL_GetBooleanProperty(props, SDL_PROP_GAMEPAD_CAP_RGB_LED_BOOLEAN, false);
        bool hasMonoLED = SDL_GetBooleanProperty(props, SDL_PROP_GAMEPAD_CAP_MONO_LED_BOOLEAN, false);
        if (!hasRGBLED && !hasMonoLED) {
            continue;
        }

        for (int32_t button = SDL_GAMEPAD_BUTTON_SOUTH; button < SDL_GAMEPAD_BUTTON_COUNT; button++) {
            if (SDL_GetGamepadButton(gamepad, static_cast<SDL_GamepadButton>(button))) {
                mapping = std::make_shared<SDLLEDMapping>(portIndex, 0, Color_RGB8({ 0, 0, 0 }));
                break;
            }
        }

        if (mapping != nullptr) {
            break;
        }

        for (int32_t i = SDL_GAMEPAD_AXIS_LEFTX; i < SDL_GAMEPAD_AXIS_COUNT; i++) {
            const auto axis = static_cast<SDL_GamepadAxis>(i);
            const auto axisValue = SDL_GetGamepadAxis(gamepad, axis) / 32767.0f;
            int32_t axisDirection = 0;
            if (axisValue < -0.7f) {
                axisDirection = NEGATIVE;
            } else if (axisValue > 0.7f) {
                axisDirection = POSITIVE;
            }

            if (axisDirection == 0) {
                continue;
            }

            mapping = std::make_shared<SDLLEDMapping>(portIndex, 0, Color_RGB8({ 0, 0, 0 }));
            break;
        }
    }

    return mapping;
}
} // namespace Ship
