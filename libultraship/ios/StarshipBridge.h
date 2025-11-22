//
//  StarshipBridge.h
//  Starship iOS Bridge
//
//  Bridge header for calling C++ mobile implementation from Objective-C++
//  This allows the iOS ViewController to communicate with the game engine
//

#ifndef StarshipBridge_h
#define StarshipBridge_h

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// Virtual Controller Functions
// ============================================================================

/**
 * Attach a virtual controller for touch input simulation
 * Creates an SDL virtual joystick that processes touch events as controller input
 */
void iOS_AttachController(void);

/**
 * Detach the virtual controller
 * Called when physical controller is connected or app goes to background
 */
void iOS_DetachController(void);

// ============================================================================
// Input Functions
// ============================================================================

/**
 * Set the state of a button on the virtual controller
 * @param button Button index (0-17), negative values are treated as axes
 * @param value True for pressed, false for released
 */
void iOS_SetButton(int button, bool value);

/**
 * Set the value of an analog axis on the virtual controller
 * @param axis Axis index (0-5): 0=LX, 1=LY, 2=RX, 3=RY, 4=LT, 5=RT
 * @param value Axis value from -32768 to 32767
 */
void iOS_SetAxis(int axis, short value);

/**
 * Set camera orientation from gyroscope or touch drag input
 * @param axis 0 for yaw (horizontal rotation), 1 for pitch (vertical rotation)
 * @param value Rotation value (typically in radians or normalized -1 to 1)
 */
void iOS_SetCameraState(int axis, float value);

// ============================================================================
// File Picker Functions
// ============================================================================

/**
 * Show iOS file picker and return selected file path
 * This function blocks until user selects a file or cancels
 * @param outPath Buffer to store selected path (caller must provide buffer of at least 1024 bytes)
 * @param pathSize Size of the output buffer
 * @return true if file was selected, false if cancelled
 */
bool iOS_ShowFilePicker(char* outPath, size_t pathSize);

// ============================================================================
// View Integration Functions
// ============================================================================

/**
 * Integrate SDL's window with iOS touch controls
 * Must be called after SDL window is created
 * @param sdlWindow Pointer to SDL_Window
 */
void iOS_IntegrateSDLView(void* sdlWindow);

// ============================================================================
// Button Mapping Constants
// ============================================================================
// These match SDL_GameControllerButton values for consistency

#define IOS_BUTTON_A             0
#define IOS_BUTTON_B             1
#define IOS_BUTTON_X             2
#define IOS_BUTTON_Y             3
#define IOS_BUTTON_BACK          4
#define IOS_BUTTON_GUIDE         5
#define IOS_BUTTON_START         6
#define IOS_BUTTON_LEFTSTICK     7
#define IOS_BUTTON_RIGHTSTICK    8
#define IOS_BUTTON_LEFTSHOULDER  9
#define IOS_BUTTON_RIGHTSHOULDER 10
#define IOS_BUTTON_DPAD_UP       11
#define IOS_BUTTON_DPAD_DOWN     12
#define IOS_BUTTON_DPAD_LEFT     13
#define IOS_BUTTON_DPAD_RIGHT    14

// ============================================================================
// Axis Mapping Constants
// ============================================================================
// These match SDL_GameControllerAxis values

#define IOS_AXIS_LEFTX        0
#define IOS_AXIS_LEFTY        1
#define IOS_AXIS_RIGHTX       2
#define IOS_AXIS_RIGHTY       3
#define IOS_AXIS_TRIGGERLEFT  4
#define IOS_AXIS_TRIGGERRIGHT 5

// ============================================================================
// Camera Axis Constants
// ============================================================================

#define IOS_CAMERA_YAW   0  // Horizontal rotation
#define IOS_CAMERA_PITCH 1  // Vertical rotation

// ============================================================================
// Menu State Functions
// ============================================================================

/**
 * Set whether the ImGui menu is currently visible
 * When visible, touch controls overlay passes touches through to SDL/ImGui
 * @param menuOpen True if menu is visible, false otherwise
 */
void iOS_SetMenuOpen(bool menuOpen);

#ifdef __cplusplus
}
#endif

#endif /* StarshipBridge_h */
