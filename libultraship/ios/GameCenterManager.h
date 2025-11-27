//
//  GameCenterManager.h
//  Starship iOS
//
//  Game Center integration for leaderboards and achievements
//  This feature is optional - users can enable/disable it in settings
//

#import <Foundation/Foundation.h>
#import <GameKit/GameKit.h>

NS_ASSUME_NONNULL_BEGIN

// Leaderboard IDs - configure these in App Store Connect
extern NSString * const kLeaderboardHighScore;
extern NSString * const kLeaderboardCorneria;
extern NSString * const kLeaderboardMeteo;
extern NSString * const kLeaderboardFichina;
extern NSString * const kLeaderboardSectorX;
extern NSString * const kLeaderboardSectorY;
extern NSString * const kLeaderboardSectorZ;
extern NSString * const kLeaderboardTitania;
extern NSString * const kLeaderboardBolse;
extern NSString * const kLeaderboardKatina;
extern NSString * const kLeaderboardSolarSystem;
extern NSString * const kLeaderboardMacbeth;
extern NSString * const kLeaderboardArea6;
extern NSString * const kLeaderboardZoness;
extern NSString * const kLeaderboardAquas;
extern NSString * const kLeaderboardVenom;

// Achievement IDs - configure these in App Store Connect
extern NSString * const kAchievementBeatCorneria;
extern NSString * const kAchievementBeatGame;
extern NSString * const kAchievementMedalCorneria;
extern NSString * const kAchievementAllMedals;
extern NSString * const kAchievementNoDamageLevel;
extern NSString * const kAchievementAllPaths;
extern NSString * const kAchievementBarrelRollMaster;
extern NSString * const kAchievementWingmanSaver;

@interface GameCenterManager : NSObject

// Singleton instance
+ (instancetype)sharedManager;

// Authentication
@property (nonatomic, readonly) BOOL isAuthenticated;
@property (nonatomic, readonly) BOOL isEnabled;
@property (nonatomic, strong, nullable) GKLocalPlayer *localPlayer;

// Enable/disable Game Center (user preference)
- (void)setEnabled:(BOOL)enabled;

// Authenticate the local player
// Call this early in app startup if Game Center is enabled
- (void)authenticateWithCompletion:(nullable void(^)(BOOL success, NSError * _Nullable error))completion;

// Leaderboards
- (void)submitScore:(int64_t)score toLeaderboard:(NSString *)leaderboardID;
- (void)submitScore:(int64_t)score toLeaderboard:(NSString *)leaderboardID withCompletion:(nullable void(^)(BOOL success, NSError * _Nullable error))completion;

// Achievements
- (void)unlockAchievement:(NSString *)achievementID;
- (void)unlockAchievement:(NSString *)achievementID withCompletion:(nullable void(^)(BOOL success, NSError * _Nullable error))completion;
- (void)reportAchievementProgress:(NSString *)achievementID percentComplete:(double)percent;

// UI
- (void)showLeaderboards;
- (void)showLeaderboard:(NSString *)leaderboardID;
- (void)showAchievements;
- (void)showGameCenterDashboard;

@end

NS_ASSUME_NONNULL_END
