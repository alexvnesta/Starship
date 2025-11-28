//
//  StarshipBridge.h
//  Starship iOS Bridge
//
//  Bridge header for calling C++ mobile implementation from Objective-C++
//  This allows the iOS ViewController to communicate with the game engine
//

#ifndef StarshipBridge_h
#define StarshipBridge_h

#include <stddef.h>
#include <stdint.h>

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

/**
 * Import an O2R asset file from user selection
 * Shows a file picker filtered for .o2r files, copies selected file to Documents
 * @param filename Target filename (e.g., "sf64.o2r" or "starship.o2r")
 * @return 0 on success, 1 if cancelled, -1 on error
 */
int iOS_ImportO2RFile(const char* filename);

/**
 * Check if an O2R file exists in the Documents directory
 * @param filename The filename to check (e.g., "sf64.o2r")
 * @return true if the file exists
 */
bool iOS_O2RFileExists(const char* filename);

/**
 * Get the Documents directory path
 * @param outPath Buffer to store path (caller must provide buffer of at least 1024 bytes)
 * @param pathSize Size of the output buffer
 * @return true if path was retrieved successfully
 */
bool iOS_GetDocumentsPath(char* outPath, size_t pathSize);

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

// ============================================================================
// Game Center Functions (Optional)
// ============================================================================

/**
 * Check if Game Center is enabled by user preference
 * @return true if Game Center is enabled
 */
bool iOS_GameCenterIsEnabled(void);

/**
 * Check if Game Center is authenticated (player signed in)
 * @return true if player is authenticated with Game Center
 */
bool iOS_GameCenterIsAuthenticated(void);

/**
 * Enable or disable Game Center
 * @param enabled true to enable, false to disable
 */
void iOS_GameCenterSetEnabled(bool enabled);

/**
 * Authenticate with Game Center
 * Should be called early in app startup if Game Center is enabled
 */
void iOS_GameCenterAuthenticate(void);

/**
 * Submit a score to a leaderboard
 * @param score The score value
 * @param leaderboardID The leaderboard identifier (see GameCenterManager.h)
 */
void iOS_GameCenterSubmitScore(int64_t score, const char* leaderboardID);

/**
 * Unlock an achievement
 * @param achievementID The achievement identifier (see GameCenterManager.h)
 */
void iOS_GameCenterUnlockAchievement(const char* achievementID);

/**
 * Report progress towards an achievement
 * @param achievementID The achievement identifier
 * @param percentComplete Progress from 0.0 to 100.0
 */
void iOS_GameCenterReportAchievementProgress(const char* achievementID, double percentComplete);

/**
 * Show the Game Center leaderboards UI
 */
void iOS_GameCenterShowLeaderboards(void);

/**
 * Show the Game Center achievements UI
 */
void iOS_GameCenterShowAchievements(void);

/**
 * Show the Game Center dashboard
 */
void iOS_GameCenterShowDashboard(void);

// ============================================================================
// Leaderboard ID Constants
// ============================================================================

#define IOS_LEADERBOARD_HIGH_SCORE    "com.starship.ios.leaderboard.highscore"
#define IOS_LEADERBOARD_CORNERIA      "com.starship.ios.leaderboard.corneria"
#define IOS_LEADERBOARD_METEO         "com.starship.ios.leaderboard.meteo"
#define IOS_LEADERBOARD_FICHINA       "com.starship.ios.leaderboard.fichina"
#define IOS_LEADERBOARD_SECTOR_X      "com.starship.ios.leaderboard.sectorx"
#define IOS_LEADERBOARD_SECTOR_Y      "com.starship.ios.leaderboard.sectory"
#define IOS_LEADERBOARD_SECTOR_Z      "com.starship.ios.leaderboard.sectorz"
#define IOS_LEADERBOARD_TITANIA       "com.starship.ios.leaderboard.titania"
#define IOS_LEADERBOARD_BOLSE         "com.starship.ios.leaderboard.bolse"
#define IOS_LEADERBOARD_KATINA        "com.starship.ios.leaderboard.katina"
#define IOS_LEADERBOARD_SOLAR         "com.starship.ios.leaderboard.solar"
#define IOS_LEADERBOARD_MACBETH       "com.starship.ios.leaderboard.macbeth"
#define IOS_LEADERBOARD_AREA6         "com.starship.ios.leaderboard.area6"
#define IOS_LEADERBOARD_ZONESS        "com.starship.ios.leaderboard.zoness"
#define IOS_LEADERBOARD_AQUAS         "com.starship.ios.leaderboard.aquas"
#define IOS_LEADERBOARD_VENOM         "com.starship.ios.leaderboard.venom"

// ============================================================================
// Achievement ID Constants
// ============================================================================

#define IOS_ACHIEVEMENT_BEAT_CORNERIA   "com.starship.ios.achievement.beat_corneria"
#define IOS_ACHIEVEMENT_BEAT_GAME       "com.starship.ios.achievement.beat_game"
#define IOS_ACHIEVEMENT_MEDAL_CORNERIA  "com.starship.ios.achievement.medal_corneria"
#define IOS_ACHIEVEMENT_ALL_MEDALS      "com.starship.ios.achievement.all_medals"
#define IOS_ACHIEVEMENT_NO_DAMAGE       "com.starship.ios.achievement.no_damage"
#define IOS_ACHIEVEMENT_ALL_PATHS       "com.starship.ios.achievement.all_paths"
#define IOS_ACHIEVEMENT_BARREL_ROLL     "com.starship.ios.achievement.barrel_roll"
#define IOS_ACHIEVEMENT_WINGMAN_SAVER   "com.starship.ios.achievement.wingman_saver"

#ifdef __cplusplus
}
#endif

#endif /* StarshipBridge_h */
