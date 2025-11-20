#if defined(__ANDROID__) || defined(__IOS__)
#include "MobileImpl.h"
#include <SDL3/SDL.h>
#include "public/bridge/consolevariablebridge.h"
#include "Context.h"
#include "controller/controldeck/ControlDeck.h"
#include "controller/physicaldevice/ConnectedPhysicalDeviceManager.h"

#include <imgui_internal.h>

static float cameraYaw;
static float cameraPitch;

static bool isShowingVirtualKeyboard = true;
static bool isUsingTouchscreenControls = true;

// STATE CACHE: Bypasses SDL3's broken virtual joystick state queries
// SDL3's virtual joystick cannot be queried immediately after setting button/axis state
// This cache provides direct, reliable access to button and axis states for iOS touch controls
#define MAX_BUTTONS 32
#define MAX_AXES 6
static bool g_button_state_cache[MAX_BUTTONS] = {false};
static int16_t g_axis_state_cache[MAX_AXES] = {0};

void Ship::Mobile::ImGuiProcessEvent(bool wantsTextInput) {
    ImGuiInputTextState* state = ImGui::GetInputTextState(ImGui::GetActiveID());

    if (wantsTextInput) {
        if (!isShowingVirtualKeyboard) {
            state->ClearText();

            isShowingVirtualKeyboard = true;
            SDL_StartTextInput(nullptr); // SDL3: pass nullptr for default window
        }
    } else {
        if (isShowingVirtualKeyboard) {
            isShowingVirtualKeyboard = false;
            SDL_StopTextInput(nullptr); // SDL3: pass nullptr for default window
        }
    }
}

#endif
#ifdef __ANDROID__
#include <jni.h>

bool Ship::Mobile::IsUsingTouchscreenControls(){
    return isUsingTouchscreenControls;
}

void Ship::Mobile::EnableTouchArea(){
    JNIEnv* env = (JNIEnv*)SDL_AndroidGetJNIEnv();
    jobject javaObject = (jobject)SDL_AndroidGetActivity();
    jclass javaClass = env->GetObjectClass(javaObject);
    jmethodID enabletoucharea = env->GetMethodID(javaClass, "EnableTouchArea", "()V");
    env->CallVoidMethod(javaObject, enabletoucharea);
}

void Ship::Mobile::DisableTouchArea(){
    JNIEnv* env = (JNIEnv*)SDL_AndroidGetJNIEnv();
    jobject javaObject = (jobject)SDL_AndroidGetActivity();
    jclass javaClass = env->GetObjectClass(javaObject);
    jmethodID disabletoucharea = env->GetMethodID(javaClass, "DisableTouchArea", "()V");
    env->CallVoidMethod(javaObject, disabletoucharea);
}

float Ship::Mobile::GetCameraYaw(){
    return cameraYaw;
}

float Ship::Mobile::GetCameraPitch(){
    return cameraPitch;
}

static int virtual_joystick_id = -1;
static SDL_Joystick *virtual_joystick = nullptr;

extern "C" void JNICALL Java_com_starship_android_MainActivity_attachController(JNIEnv* env, jobject obj) {
    // SDL3: Create virtual joystick descriptor
    SDL_VirtualJoystickDesc desc;
    SDL_INIT_INTERFACE(&desc);
    desc.type = SDL_JOYSTICK_TYPE_GAMEPAD;
    desc.naxes = 6;
    desc.nbuttons = 18;
    desc.nhats = 0;

    virtual_joystick_id = SDL_AttachVirtualJoystick(&desc);
    if (virtual_joystick_id == 0) {
            SDL_Log("Could not create overlay virtual controller");
        return;
    }

    virtual_joystick = SDL_OpenJoystick(virtual_joystick_id);
    if (virtual_joystick == nullptr)
        SDL_Log("Could not create virtual joystick");

    isUsingTouchscreenControls = true;
}

extern "C" void JNICALL Java_com_starship_android_MainActivity_setCameraState(JNIEnv *env, jobject jobj, jint axis, jfloat value) {
    switch(axis){
        case 0:
            cameraYaw=value;
            break;
        case 1:
            cameraPitch=value;
            break;
    }
}

extern "C" void JNICALL Java_com_starship_android_MainActivity_setButton(JNIEnv *env, jobject jobj, jint button, jboolean value) {
    if(button < 0){
        SDL_SetJoystickVirtualAxis(virtual_joystick,-button, value ? SDL_MAX_SINT16 : -SDL_MAX_SINT16); // This should be 0 when false, but I think there's a bug in SDL
    }else{
        SDL_SetJoystickVirtualButton(virtual_joystick, button, value);
    }
}

#include "Context.h"

extern "C" void JNICALL Java_com_starship_android_MainActivity_setAxis(JNIEnv *env, jobject jobj, jint axis, jshort value) {
    SDL_SetJoystickVirtualAxis(virtual_joystick, axis, value);
}

extern "C" void JNICALL Java_com_starship_android_MainActivity_detachController(JNIEnv *env, jobject jobj) {
    SDL_CloseJoystick(virtual_joystick);
    SDL_DetachVirtualJoystick(virtual_joystick_id);
    virtual_joystick = nullptr;
    virtual_joystick_id = -1;
    isUsingTouchscreenControls = false;
}

extern "C" JNIEXPORT void JNICALL
Java_com_starship_android_MainActivity_nativeHandleSelectedFile(JNIEnv* env, jobject thiz, jstring filename) {
}

#endif // __ANDROID__

// ============================================================================
// iOS Implementation
// ============================================================================
#ifdef __IOS__
// Virtual joystick state for iOS touch controls
static int virtual_joystick_id = -1;
static SDL_Joystick *virtual_joystick = nullptr;
static SDL_Gamepad *virtual_gamepad = nullptr;

// iOS-specific touch control functions
bool Ship::Mobile::IsUsingTouchscreenControls() {
    return isUsingTouchscreenControls;
}

void Ship::Mobile::EnableTouchArea() {
    // On iOS, touch controls are managed directly through SDL events
    // This function is kept for API compatibility with Android
    // The actual touch overlay will be implemented in the ViewController
}

void Ship::Mobile::DisableTouchArea() {
    // On iOS, touch controls are managed directly through SDL events
    // This function is kept for API compatibility with Android
}

float Ship::Mobile::GetCameraYaw() {
    return cameraYaw;
}

float Ship::Mobile::GetCameraPitch() {
    return cameraPitch;
}

// iOS virtual controller attachment
// Called when touch controls are enabled
extern "C" void iOS_AttachController() {
    SDL_Log("[iOS] ========== iOS_AttachController called ==========");

    // SDL3: Create virtual joystick descriptor
    // Create a virtual joystick with 6 axes and 18 buttons
    // This matches the Android implementation for consistency
    SDL_VirtualJoystickDesc desc;
    SDL_INIT_INTERFACE(&desc);
    desc.type = SDL_JOYSTICK_TYPE_GAMEPAD;
    desc.naxes = 6;
    desc.nbuttons = 18;
    desc.nhats = 0;

    // CRITICAL: Set button_mask to map raw joystick button indices to SDL gamepad button types
    // This tells SDL3 which standard gamepad buttons our virtual controller supports
    // Without this, SDL_GetGamepadButton() won't know how to map button indices to gamepad buttons
    // NOTE: This mask indicates which SDL_GAMEPAD_BUTTON types are supported (not button indices!)
    // The mapping string below maps button indices (b0, b1, ...) to these gamepad button types
    desc.button_mask = (1 << SDL_GAMEPAD_BUTTON_SOUTH) |      // Support A button (SOUTH)
                       (1 << SDL_GAMEPAD_BUTTON_EAST) |       // Support B button (EAST)
                       (1 << SDL_GAMEPAD_BUTTON_WEST) |       // Support X button (WEST)
                       (1 << SDL_GAMEPAD_BUTTON_NORTH) |      // Support Y button (NORTH)
                       (1 << SDL_GAMEPAD_BUTTON_BACK) |       // Support Back/Select button
                       (1 << SDL_GAMEPAD_BUTTON_GUIDE) |      // Support Guide button (CRITICAL: needed for guide:b5 in mapping)
                       (1 << SDL_GAMEPAD_BUTTON_START) |      // Support Start button (CRITICAL: needed for start:b6 in mapping)
                       (1 << SDL_GAMEPAD_BUTTON_LEFT_STICK) | // Support Left Stick button (L3)
                       (1 << SDL_GAMEPAD_BUTTON_RIGHT_STICK) | // Support Right Stick button (R3)
                       (1 << SDL_GAMEPAD_BUTTON_LEFT_SHOULDER) | // Support Left Shoulder button (L1)
                       (1 << SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER) | // Support Right Shoulder button (R1)
                       (1 << SDL_GAMEPAD_BUTTON_DPAD_UP) |    // Support D-Pad Up
                       (1 << SDL_GAMEPAD_BUTTON_DPAD_DOWN) |  // Support D-Pad Down
                       (1 << SDL_GAMEPAD_BUTTON_DPAD_LEFT) |  // Support D-Pad Left
                       (1 << SDL_GAMEPAD_BUTTON_DPAD_RIGHT);  // Support D-Pad Right
    SDL_Log("[iOS] Set button_mask: 0x%X (supports standard gamepad buttons)", desc.button_mask);

    SDL_Log("[iOS] Calling SDL_AttachVirtualJoystick with %d axes, %d buttons...", desc.naxes, desc.nbuttons);
    virtual_joystick_id = SDL_AttachVirtualJoystick(&desc);
    SDL_Log("[iOS] SDL_AttachVirtualJoystick returned ID: %u", virtual_joystick_id);

    if (virtual_joystick_id == 0) {
        SDL_Log("[iOS] ERROR: Could not create overlay virtual controller - SDL_AttachVirtualJoystick returned 0");
        SDL_Log("[iOS] SDL Error: %s", SDL_GetError());
        return;
    }

    SDL_Log("[iOS] Calling SDL_OpenJoystick with ID %u...", virtual_joystick_id);
    virtual_joystick = SDL_OpenJoystick(virtual_joystick_id);
    SDL_Log("[iOS] SDL_OpenJoystick returned: %p", virtual_joystick);

    if (virtual_joystick == nullptr) {
        SDL_Log("[iOS] ERROR: Could not open virtual joystick");
        SDL_Log("[iOS] SDL Error: %s", SDL_GetError());
        return;
    }

    // CRITICAL: Also open as gamepad so the game's input system recognizes button presses
    // In SDL3, games typically use SDL_Gamepad API which provides standard button mapping
    SDL_Log("[iOS] Calling SDL_OpenGamepad with ID %u...", virtual_joystick_id);
    virtual_gamepad = SDL_OpenGamepad(virtual_joystick_id);
    SDL_Log("[iOS] SDL_OpenGamepad returned: %p", virtual_gamepad);

    if (virtual_gamepad == nullptr) {
        SDL_Log("[iOS] ERROR: Could not open virtual gamepad");
        SDL_Log("[iOS] SDL Error: %s", SDL_GetError());
        // Continue anyway - joystick might still work
    } else {
        SDL_Log("[iOS] ✅ Virtual controller opened as BOTH joystick and gamepad!");
    }

    isUsingTouchscreenControls = true;
    SDL_Log("[iOS] ========== Virtual controller attached successfully! ==========");
    SDL_Log("[iOS] virtual_joystick_id=%u, virtual_joystick=%p, virtual_gamepad=%p", virtual_joystick_id, virtual_joystick, virtual_gamepad);

    // CRITICAL: Refresh the game's controller manager so it detects our virtual gamepad
    // The ConnectedPhysicalDeviceManager scans for gamepads at startup, but our virtual
    // gamepad is created after that initial scan (when touch controls are enabled).
    // We need to trigger a refresh so it gets added to the manager's internal map.

    // Add the gamepad mapping string BEFORE checking if it's a gamepad
    // This tells SDL how to map joystick button indices (0, 1, 2...) to gamepad button types (SOUTH, EAST, WEST...)
    // Without this mapping, SDL knows which button TYPES are available (from button_mask)
    // but doesn't know HOW to map button indices to button types
    SDL_Log("[iOS] Adding gamepad mapping string to map button indices to gamepad button types...");

    // Format: "GUID,name,mapping"
    // This maps: button index 0 = 'a' (SOUTH), button index 1 = 'b' (EAST), etc.
    // MUST match the button indices defined in StarshipBridge.h!
    const char* mapping = "03000000000000000000000000000000,Virtual iOS Controller,"
                         "a:b0,b:b1,x:b2,y:b3,"
                         "back:b4,guide:b5,start:b6,"
                         "leftstick:b7,rightstick:b8,"
                         "leftshoulder:b9,rightshoulder:b10,"
                         "dpup:b11,dpdown:b12,dpleft:b13,dpright:b14,"
                         "leftx:a0,lefty:a1,rightx:a2,righty:a3,"
                         "lefttrigger:a4,righttrigger:a5,";

    int result = SDL_AddGamepadMapping(mapping);
    SDL_Log("[iOS] SDL_AddGamepadMapping() returned: %d (1=success, 0=updated, -1=error)", result);

    if (result < 0) {
        SDL_Log("[iOS] SDL_AddGamepadMapping error: %s", SDL_GetError());
    } else {
        SDL_Log("[iOS] ✅ Gamepad mapping successfully added!");
    }

    // DIAGNOSTIC: Check if SDL recognizes our virtual joystick as a gamepad
    SDL_Log("[iOS] ========== Diagnostic: Checking SDL gamepad recognition ==========");
    bool isGamepad = SDL_IsGamepad(virtual_joystick_id);
    SDL_Log("[iOS] SDL_IsGamepad(%u) returned: %s", virtual_joystick_id, isGamepad ? "TRUE" : "FALSE");

    if (!isGamepad) {
        SDL_Log("[iOS] ❌ CRITICAL: SDL does not recognize virtual joystick as a gamepad!");
        SDL_Log("[iOS] This is why it won't be added to the ConnectedSDLGamepads map!");
    } else {
        SDL_Log("[iOS] ✅ SDL recognizes virtual controller as a gamepad!");
    }

    SDL_Log("[iOS] Calling RefreshConnectedSDLGamepads() to register virtual gamepad with game...");
    auto controlDeck = Ship::Context::GetInstance()->GetControlDeck();
    if (controlDeck) {
        auto deviceManager = controlDeck->GetConnectedPhysicalDeviceManager();
        if (deviceManager) {
            deviceManager->RefreshConnectedSDLGamepads();
            SDL_Log("[iOS] ✅ RefreshConnectedSDLGamepads() completed!");

            // DIAGNOSTIC: Verify the virtual gamepad was actually added to the map
            SDL_Log("[iOS] ========== Diagnostic: Verifying gamepad in map ==========");
            auto gamepadNames = deviceManager->GetConnectedSDLGamepadNames();
            SDL_Log("[iOS] Total gamepads in map: %zu", gamepadNames.size());

            bool foundVirtual = false;
            SDL_JoystickID virtualInstanceId = SDL_GetGamepadID(virtual_gamepad);
            SDL_Log("[iOS] Our virtual gamepad instance ID: %u", virtualInstanceId);

            for (const auto& [instanceId, name] : gamepadNames) {
                SDL_Log("[iOS] Gamepad in map - ID: %u, Name: %s", instanceId, name.c_str());

                // Check if this is our virtual gamepad by comparing instance IDs
                if (instanceId == virtualInstanceId) {
                    foundVirtual = true;
                    SDL_Log("[iOS] ✅ FOUND our virtual gamepad in the map! ID=%u", instanceId);
                }
            }

            if (!foundVirtual) {
                SDL_Log("[iOS] ❌ CRITICAL: Virtual gamepad NOT in map!");
                SDL_Log("[iOS] This means SDL_IsGamepad() returned false");
                SDL_Log("[iOS] The game won't see button presses from the virtual controller");
            } else {
                SDL_Log("[iOS] ✅ Virtual gamepad successfully registered with game!");
            }
        } else {
            SDL_Log("[iOS] ❌ ERROR: Could not get ConnectedPhysicalDeviceManager!");
        }
    } else {
        SDL_Log("[iOS] ❌ ERROR: Could not get ControlDeck!");
    }
}

// iOS virtual controller detachment
// Called when physical controller is connected or touch controls disabled
extern "C" void iOS_DetachController() {
    if (virtual_gamepad != nullptr) {
        SDL_CloseGamepad(virtual_gamepad);
        virtual_gamepad = nullptr;
        SDL_Log("[iOS] Virtual gamepad closed");
    }

    if (virtual_joystick != nullptr) {
        SDL_CloseJoystick(virtual_joystick);
        SDL_DetachVirtualJoystick(virtual_joystick_id);
        virtual_joystick = nullptr;
        virtual_joystick_id = -1;
        isUsingTouchscreenControls = false;
        SDL_Log("[iOS] Virtual controller detached");
    }
}

// Set button state from touch input
// button: button index (negative values represent axes)
// value: true for pressed, false for released
extern "C" void iOS_SetButton(int button, bool value) {
    if (virtual_joystick == nullptr) {
        SDL_Log("[iOS] ERROR: iOS_SetButton called but virtual_joystick is NULL - controller not attached!");
        return;
    }

    SDL_Log("[iOS] iOS_SetButton: button=%d, value=%d, joystick=%p", button, value, virtual_joystick);

    if (button < 0) {
        // Negative button values are treated as axis inputs
        // Convert boolean to axis value (-32768 to 32767)
        bool result = SDL_SetJoystickVirtualAxis(virtual_joystick, -button,
                                   value ? SDL_MAX_SINT16 : -SDL_MAX_SINT16);
        SDL_Log("[iOS] SDL_SetJoystickVirtualAxis(axis=%d, value=%d) returned: %s",
                -button, value ? SDL_MAX_SINT16 : -SDL_MAX_SINT16,
                result ? "SUCCESS" : "FAILURE");
        if (!result) {
            SDL_Log("[iOS] SDL Error: %s", SDL_GetError());
        }
    } else {
        // CRITICAL FIX: Update button state cache FIRST
        // SDL3's virtual joystick state queries are broken - they can't be queried immediately after setting
        // This cache provides direct, reliable access to button states bypassing SDL's broken queries
        if (button >= 0 && button < MAX_BUTTONS) {
            g_button_state_cache[button] = value;
            SDL_Log("[iOS] ✅ Button state cache updated: button=%d, value=%d", button, value);
        } else {
            SDL_Log("[iOS] ❌ WARNING: Button index %d out of range [0-%d]", button, MAX_BUTTONS - 1);
        }

        // Still call SDL APIs for compatibility but don't rely on them for state queries
        bool result = SDL_SetJoystickVirtualButton(virtual_joystick, button, value);
        SDL_Log("[iOS] SDL_SetJoystickVirtualButton(button=%d, value=%d) returned: %s",
                button, value, result ? "SUCCESS" : "FAILURE");

        if (!result) {
            SDL_Log("[iOS] ❌ SDL_SetJoystickVirtualButton FAILED!");
            SDL_Log("[iOS] SDL Error: %s", SDL_GetError());
        }

        // Still pump events for compatibility but we no longer rely on SDL state queries
        SDL_PumpEvents();
        SDL_UpdateJoysticks();
    }
}

// Query button state from cache
// This bypasses SDL3's broken virtual joystick state queries
// button: button index (0-31)
// Returns: true if button is pressed, false if released or invalid
extern "C" bool iOS_GetButtonState(int button) {
    if (button < 0 || button >= MAX_BUTTONS) {
        SDL_Log("[iOS] iOS_GetButtonState: Invalid button index %d (valid range: 0-%d)", button, MAX_BUTTONS - 1);
        return false;
    }
    return g_button_state_cache[button];
}

// Set analog stick axis value from touch input
// axis: axis index (0-5)
// value: axis value (-32768 to 32767)
extern "C" void iOS_SetAxis(int axis, short value) {
    if (virtual_joystick == nullptr) {
        SDL_Log("[iOS] ERROR: iOS_SetAxis called but virtual_joystick is NULL!");
        return;
    }

    // CRITICAL FIX: Update axis state cache FIRST
    // SDL3's virtual joystick axis queries are broken just like button queries
    // This cache provides direct, reliable access to axis states bypassing SDL's broken queries
    if (axis >= 0 && axis < MAX_AXES) {
        g_axis_state_cache[axis] = value;

        // Only log when axis value changes significantly (avoid spam)
        static short lastValues[MAX_AXES] = {0};
        if (abs(value - lastValues[axis]) > 1000) {
            SDL_Log("[iOS] ✅ Axis state cache updated: axis=%d, value=%d", axis, value);
            lastValues[axis] = value;
        }
    } else {
        SDL_Log("[iOS] ❌ WARNING: Axis index %d out of range [0-%d]", axis, MAX_AXES - 1);
    }

    // Still call SDL API for compatibility but don't rely on it for state queries
    SDL_SetJoystickVirtualAxis(virtual_joystick, axis, value);
}

// Query axis state from cache
// This bypasses SDL3's broken virtual joystick state queries
// axis: axis index (0-5)
// Returns: axis value (-32768 to 32767), or 0 if invalid
extern "C" int16_t iOS_GetAxisState(int axis) {
    if (axis < 0 || axis >= MAX_AXES) {
        SDL_Log("[iOS] iOS_GetAxisState: Invalid axis index %d (valid range: 0-%d)", axis, MAX_AXES - 1);
        return 0;
    }
    return g_axis_state_cache[axis];
}

// Set camera orientation from gyroscope or touch drag
// axis: 0 for yaw (horizontal), 1 for pitch (vertical)
// value: rotation value in radians or degrees
extern "C" void iOS_SetCameraState(int axis, float value) {
    switch(axis) {
        case 0:
            cameraYaw = value;
            break;
        case 1:
            cameraPitch = value;
            break;
        default:
            SDL_Log("[iOS] Invalid camera axis: %d", axis);
            break;
    }
}

#endif // __IOS__
