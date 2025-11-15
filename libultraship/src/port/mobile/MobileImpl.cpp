#if defined(__ANDROID__) || defined(__IOS__)
#include "MobileImpl.h"
#include <SDL2/SDL.h>
#include "public/bridge/consolevariablebridge.h"

#include <imgui_internal.h>

static float cameraYaw;
static float cameraPitch;

static bool isShowingVirtualKeyboard = true;
static bool isUsingTouchscreenControls = true;

void Ship::Mobile::ImGuiProcessEvent(bool wantsTextInput) {
    ImGuiInputTextState* state = ImGui::GetInputTextState(ImGui::GetActiveID());

    if (wantsTextInput) {
        if (!isShowingVirtualKeyboard) {
            state->ClearText();

            isShowingVirtualKeyboard = true;
            SDL_StartTextInput();
        }
    } else {
        if (isShowingVirtualKeyboard) {
            isShowingVirtualKeyboard = false;
            SDL_StopTextInput();
        }
    }
}

#endif
#ifdef __ANDROID__
#include <SDL_gamecontroller.h>
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
    virtual_joystick_id = SDL_JoystickAttachVirtual(SDL_JOYSTICK_TYPE_GAMECONTROLLER, 6, 18, 0);
    if (virtual_joystick_id == -1) {
            SDL_Log("Could not create overlay virtual controller");
        return;
    }

    virtual_joystick = SDL_JoystickOpen(virtual_joystick_id);
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
        SDL_JoystickSetVirtualAxis(virtual_joystick,-button, value ? SDL_MAX_SINT16 : -SDL_MAX_SINT16); // This should be 0 when false, but I think there's a bug in SDL
    }else{
        SDL_JoystickSetVirtualButton(virtual_joystick, button, value);
    }
}

#include "Context.h"

extern "C" void JNICALL Java_com_starship_android_MainActivity_setAxis(JNIEnv *env, jobject jobj, jint axis, jshort value) {
    SDL_JoystickSetVirtualAxis(virtual_joystick, axis, value);
}

extern "C" void JNICALL Java_com_starship_android_MainActivity_detachController(JNIEnv *env, jobject jobj) {
    SDL_JoystickClose(virtual_joystick);
    SDL_JoystickDetachVirtual(virtual_joystick_id);
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
#include <SDL_gamecontroller.h>

// Virtual joystick state for iOS touch controls
static int virtual_joystick_id = -1;
static SDL_Joystick *virtual_joystick = nullptr;

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
    // Create a virtual joystick with 6 axes and 18 buttons
    // This matches the Android implementation for consistency
    virtual_joystick_id = SDL_JoystickAttachVirtual(SDL_JOYSTICK_TYPE_GAMECONTROLLER, 6, 18, 0);
    if (virtual_joystick_id == -1) {
        SDL_Log("[iOS] Could not create overlay virtual controller");
        return;
    }

    virtual_joystick = SDL_JoystickOpen(virtual_joystick_id);
    if (virtual_joystick == nullptr) {
        SDL_Log("[iOS] Could not open virtual joystick");
        return;
    }

    isUsingTouchscreenControls = true;
    SDL_Log("[iOS] Virtual controller attached successfully");
}

// iOS virtual controller detachment
// Called when physical controller is connected or touch controls disabled
extern "C" void iOS_DetachController() {
    if (virtual_joystick != nullptr) {
        SDL_JoystickClose(virtual_joystick);
        SDL_JoystickDetachVirtual(virtual_joystick_id);
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
        return;
    }

    if (button < 0) {
        // Negative button values are treated as axis inputs
        // Convert boolean to axis value (-32768 to 32767)
        SDL_JoystickSetVirtualAxis(virtual_joystick, -button,
                                   value ? SDL_MAX_SINT16 : -SDL_MAX_SINT16);
    } else {
        SDL_JoystickSetVirtualButton(virtual_joystick, button, value);
    }
}

// Set analog stick axis value from touch input
// axis: axis index (0-5)
// value: axis value (-32768 to 32767)
extern "C" void iOS_SetAxis(int axis, short value) {
    if (virtual_joystick != nullptr) {
        SDL_JoystickSetVirtualAxis(virtual_joystick, axis, value);
    }
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
