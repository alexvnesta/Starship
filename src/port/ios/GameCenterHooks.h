//
// GameCenterHooks.h
// Game Center integration hooks for iOS
//

#ifndef GAMECENTER_HOOKS_H
#define GAMECENTER_HOOKS_H

#ifdef __cplusplus
extern "C" {
#endif

// Call this every frame from the game update loop to check for events
void GameCenter_Update(void);

// Call when a level is completed
void GameCenter_OnLevelComplete(int levelId, int hitCount, int gotMedal);

// Call when the game is beaten (Andross defeated)
void GameCenter_OnGameComplete(int totalScore);

// Call when a medal is earned
void GameCenter_OnMedalEarned(int levelId);

// Call when all medals are collected
void GameCenter_OnAllMedals(void);

#ifdef __cplusplus
}
#endif

#endif // GAMECENTER_HOOKS_H
