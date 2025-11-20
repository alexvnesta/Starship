#include "SDLAddRemoveDeviceEventHandler.h"
#include <SDL3/SDL.h>
#include "Context.h"
#include "controller/controldeck/ControlDeck.h"

namespace Ship {

SDLAddRemoveDeviceEventHandler::~SDLAddRemoveDeviceEventHandler() {
}

void SDLAddRemoveDeviceEventHandler::InitElement() {
}

void SDLAddRemoveDeviceEventHandler::DrawElement() {
}

void SDLAddRemoveDeviceEventHandler::UpdateElement() {
    SDL_PumpEvents();
    SDL_Event event;
    while (SDL_PeepEvents(&event, 1, SDL_GETEVENT, SDL_EVENT_GAMEPAD_ADDED, SDL_EVENT_GAMEPAD_ADDED) > 0) {
        // from https://wiki.libsdl.org/SDL3/SDL_GamepadDeviceEvent: which - the joystick device index for
        // the SDL_EVENT_GAMEPAD_ADDED event
        Context::GetInstance()->GetControlDeck()->GetConnectedPhysicalDeviceManager()->HandlePhysicalDeviceConnect(
            event.gdevice.which);
    }

    while (SDL_PeepEvents(&event, 1, SDL_GETEVENT, SDL_EVENT_GAMEPAD_REMOVED, SDL_EVENT_GAMEPAD_REMOVED) > 0) {
        // from https://wiki.libsdl.org/SDL3/SDL_GamepadDeviceEvent: which - the [...] instance id for the
        // SDL_EVENT_GAMEPAD_REMOVED [...] event
        Context::GetInstance()->GetControlDeck()->GetConnectedPhysicalDeviceManager()->HandlePhysicalDeviceDisconnect(
            event.gdevice.which);
    }
}
} // namespace Ship
