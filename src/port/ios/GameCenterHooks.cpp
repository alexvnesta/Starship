//
// GameCenterHooks.cpp
// Game Center integration hooks for iOS
//

#include "GameCenterHooks.h"

#ifdef __IOS__
#include "libultraship/ios/StarshipBridge.h"
#endif

// Game state externs
extern "C" {
    extern int gCurrentLevel;
    extern int gHitCount;
    extern int gMissionMedal[7];
    extern int gMissionNumber;
    extern int gMissionHitCount[7];
    extern unsigned char gShowLevelClearStatusScreen;
    extern unsigned char gMissionStatus;
    extern int gGameState;
    extern int gNextGameState;
}

// Game states (from sf64thread.h)
#define GSTATE_ENDING 8

// Level ID enum (from sf64level.h)
enum LevelId {
    LEVEL_CORNERIA = 0,
    LEVEL_METEO = 1,
    LEVEL_SECTOR_X = 2,
    LEVEL_AREA_6 = 3,
    LEVEL_UNK_4 = 4,
    LEVEL_SECTOR_Y = 5,
    LEVEL_VENOM_1 = 6,
    LEVEL_SOLAR = 7,
    LEVEL_ZONESS = 8,
    LEVEL_VENOM_ANDROSS = 9,
    LEVEL_TRAINING = 10,
    LEVEL_MACBETH = 11,
    LEVEL_TITANIA = 12,
    LEVEL_AQUAS = 13,
    LEVEL_FORTUNA = 14,
    LEVEL_UNK_15 = 15,
    LEVEL_KATINA = 16,
    LEVEL_BOLSE = 17,
    LEVEL_SECTOR_Z = 18,
    LEVEL_VENOM_2 = 19,
};

// Track state to detect transitions
static bool sWasLevelClearScreenShowing = false;
static int sLastCompletedLevel = -1;
static bool sGameCompleteSubmitted = false;
static int sLastGameState = -1;

#ifdef __IOS__
// Get the leaderboard ID for a given level
static const char* GetLeaderboardForLevel(int levelId) {
    switch (levelId) {
        case LEVEL_CORNERIA:    return IOS_LEADERBOARD_CORNERIA;
        case LEVEL_METEO:       return IOS_LEADERBOARD_METEO;
        case LEVEL_SECTOR_X:    return IOS_LEADERBOARD_SECTOR_X;
        case LEVEL_SECTOR_Y:    return IOS_LEADERBOARD_SECTOR_Y;
        case LEVEL_SECTOR_Z:    return IOS_LEADERBOARD_SECTOR_Z;
        case LEVEL_AREA_6:      return IOS_LEADERBOARD_AREA6;
        case LEVEL_SOLAR:       return IOS_LEADERBOARD_SOLAR;
        case LEVEL_ZONESS:      return IOS_LEADERBOARD_ZONESS;
        case LEVEL_AQUAS:       return IOS_LEADERBOARD_AQUAS;
        case LEVEL_TITANIA:     return IOS_LEADERBOARD_TITANIA;
        case LEVEL_MACBETH:     return IOS_LEADERBOARD_MACBETH;
        case LEVEL_FORTUNA:     return IOS_LEADERBOARD_FICHINA; // Fortuna is Fichina
        case LEVEL_KATINA:      return IOS_LEADERBOARD_KATINA;
        case LEVEL_BOLSE:       return IOS_LEADERBOARD_BOLSE;
        case LEVEL_VENOM_1:
        case LEVEL_VENOM_2:
        case LEVEL_VENOM_ANDROSS: return IOS_LEADERBOARD_VENOM;
        default: return nullptr;
    }
}
#endif

void GameCenter_Update(void) {
#ifdef __IOS__
    // Only process if Game Center is enabled and authenticated
    if (!iOS_GameCenterIsEnabled() || !iOS_GameCenterIsAuthenticated()) {
        sWasLevelClearScreenShowing = gShowLevelClearStatusScreen;
        sLastGameState = gGameState;
        return;
    }

    // Detect level clear screen showing (transition from false to true)
    if (gShowLevelClearStatusScreen && !sWasLevelClearScreenShowing) {
        // Level just completed - submit score
        if (gCurrentLevel != sLastCompletedLevel) {
            sLastCompletedLevel = gCurrentLevel;
            GameCenter_OnLevelComplete(gCurrentLevel, gHitCount, gMissionMedal[gMissionNumber]);
        }
    }

    // Detect game completion (transition to GSTATE_ENDING)
    if (gGameState == GSTATE_ENDING && sLastGameState != GSTATE_ENDING && !sGameCompleteSubmitted) {
        sGameCompleteSubmitted = true;

        // Calculate total score from all missions
        int totalScore = 0;
        for (int i = 0; i <= gMissionNumber && i < 7; i++) {
            totalScore += gMissionHitCount[i];
        }

        GameCenter_OnGameComplete(totalScore);
    }

    // Reset completion flag when starting a new game
    if (gGameState != GSTATE_ENDING && sLastGameState == GSTATE_ENDING) {
        sGameCompleteSubmitted = false;
        sLastCompletedLevel = -1;
    }

    sWasLevelClearScreenShowing = gShowLevelClearStatusScreen;
    sLastGameState = gGameState;
#endif
}

void GameCenter_OnLevelComplete(int levelId, int hitCount, int gotMedal) {
#ifdef __IOS__
    if (!iOS_GameCenterIsEnabled() || !iOS_GameCenterIsAuthenticated()) {
        return;
    }

    // Submit level score to leaderboard
    const char* leaderboardId = GetLeaderboardForLevel(levelId);
    if (leaderboardId != nullptr) {
        iOS_GameCenterSubmitScore(hitCount, leaderboardId);
    }

    // Check for achievements
    if (levelId == LEVEL_CORNERIA) {
        iOS_GameCenterUnlockAchievement(IOS_ACHIEVEMENT_BEAT_CORNERIA);

        if (gotMedal) {
            iOS_GameCenterUnlockAchievement(IOS_ACHIEVEMENT_MEDAL_CORNERIA);
        }
    }

    // Medal achievement for any level
    if (gotMedal) {
        GameCenter_OnMedalEarned(levelId);
    }
#endif
}

void GameCenter_OnGameComplete(int totalScore) {
#ifdef __IOS__
    if (!iOS_GameCenterIsEnabled() || !iOS_GameCenterIsAuthenticated()) {
        return;
    }

    // Submit total score to high score leaderboard
    iOS_GameCenterSubmitScore(totalScore, IOS_LEADERBOARD_HIGH_SCORE);

    // Unlock "beat the game" achievement
    iOS_GameCenterUnlockAchievement(IOS_ACHIEVEMENT_BEAT_GAME);
#endif
}

void GameCenter_OnMedalEarned(int levelId) {
#ifdef __IOS__
    if (!iOS_GameCenterIsEnabled() || !iOS_GameCenterIsAuthenticated()) {
        return;
    }

    // Check if all medals collected
    // Total of 15 planets that can have medals
    int medalCount = 0;
    for (int i = 0; i < 7; i++) {
        if (gMissionMedal[i]) {
            medalCount++;
        }
    }

    // Report progress (rough approximation - actual medal tracking is more complex)
    double progress = (medalCount / 15.0) * 100.0;
    iOS_GameCenterReportAchievementProgress(IOS_ACHIEVEMENT_ALL_MEDALS, progress);

    if (medalCount >= 15) {
        GameCenter_OnAllMedals();
    }
#endif
}

void GameCenter_OnAllMedals(void) {
#ifdef __IOS__
    if (!iOS_GameCenterIsEnabled() || !iOS_GameCenterIsAuthenticated()) {
        return;
    }

    iOS_GameCenterUnlockAchievement(IOS_ACHIEVEMENT_ALL_MEDALS);
#endif
}
