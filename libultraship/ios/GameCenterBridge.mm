//
//  GameCenterBridge.mm
//  Starship iOS
//
//  C bridge implementation for Game Center functions
//

#import "GameCenterManager.h"
#import "StarshipBridge.h"

extern "C" {

bool iOS_GameCenterIsEnabled(void) {
    return [[GameCenterManager sharedManager] isEnabled];
}

bool iOS_GameCenterIsAuthenticated(void) {
    return [[GameCenterManager sharedManager] isAuthenticated];
}

void iOS_GameCenterSetEnabled(bool enabled) {
    [[GameCenterManager sharedManager] setEnabled:enabled];
}

void iOS_GameCenterAuthenticate(void) {
    [[GameCenterManager sharedManager] authenticateWithCompletion:nil];
}

void iOS_GameCenterSubmitScore(int64_t score, const char* leaderboardID) {
    if (!leaderboardID) return;

    NSString *leaderboard = [NSString stringWithUTF8String:leaderboardID];
    [[GameCenterManager sharedManager] submitScore:score toLeaderboard:leaderboard];
}

void iOS_GameCenterUnlockAchievement(const char* achievementID) {
    if (!achievementID) return;

    NSString *achievement = [NSString stringWithUTF8String:achievementID];
    [[GameCenterManager sharedManager] unlockAchievement:achievement];
}

void iOS_GameCenterReportAchievementProgress(const char* achievementID, double percentComplete) {
    if (!achievementID) return;

    NSString *achievement = [NSString stringWithUTF8String:achievementID];
    [[GameCenterManager sharedManager] reportAchievementProgress:achievement percentComplete:percentComplete];
}

void iOS_GameCenterShowLeaderboards(void) {
    dispatch_async(dispatch_get_main_queue(), ^{
        [[GameCenterManager sharedManager] showLeaderboards];
    });
}

void iOS_GameCenterShowAchievements(void) {
    dispatch_async(dispatch_get_main_queue(), ^{
        [[GameCenterManager sharedManager] showAchievements];
    });
}

void iOS_GameCenterShowDashboard(void) {
    dispatch_async(dispatch_get_main_queue(), ^{
        [[GameCenterManager sharedManager] showGameCenterDashboard];
    });
}

} // extern "C"
