//
//  GameCenterManager.mm
//  Starship iOS
//
//  Game Center integration for leaderboards and achievements
//

#import "GameCenterManager.h"
#import <UIKit/UIKit.h>

// Leaderboard IDs - configure these in App Store Connect
// Format: com.starship.ios.leaderboard.<name>
NSString * const kLeaderboardHighScore = @"com.starship.ios.leaderboard.highscore";
NSString * const kLeaderboardCorneria = @"com.starship.ios.leaderboard.corneria";
NSString * const kLeaderboardMeteo = @"com.starship.ios.leaderboard.meteo";
NSString * const kLeaderboardFichina = @"com.starship.ios.leaderboard.fichina";
NSString * const kLeaderboardSectorX = @"com.starship.ios.leaderboard.sectorx";
NSString * const kLeaderboardSectorY = @"com.starship.ios.leaderboard.sectory";
NSString * const kLeaderboardSectorZ = @"com.starship.ios.leaderboard.sectorz";
NSString * const kLeaderboardTitania = @"com.starship.ios.leaderboard.titania";
NSString * const kLeaderboardBolse = @"com.starship.ios.leaderboard.bolse";
NSString * const kLeaderboardKatina = @"com.starship.ios.leaderboard.katina";
NSString * const kLeaderboardSolarSystem = @"com.starship.ios.leaderboard.solar";
NSString * const kLeaderboardMacbeth = @"com.starship.ios.leaderboard.macbeth";
NSString * const kLeaderboardArea6 = @"com.starship.ios.leaderboard.area6";
NSString * const kLeaderboardZoness = @"com.starship.ios.leaderboard.zoness";
NSString * const kLeaderboardAquas = @"com.starship.ios.leaderboard.aquas";
NSString * const kLeaderboardVenom = @"com.starship.ios.leaderboard.venom";

// Achievement IDs - configure these in App Store Connect
// Format: com.starship.ios.achievement.<name>
NSString * const kAchievementBeatCorneria = @"com.starship.ios.achievement.beat_corneria";
NSString * const kAchievementBeatGame = @"com.starship.ios.achievement.beat_game";
NSString * const kAchievementMedalCorneria = @"com.starship.ios.achievement.medal_corneria";
NSString * const kAchievementAllMedals = @"com.starship.ios.achievement.all_medals";
NSString * const kAchievementNoDamageLevel = @"com.starship.ios.achievement.no_damage";
NSString * const kAchievementAllPaths = @"com.starship.ios.achievement.all_paths";
NSString * const kAchievementBarrelRollMaster = @"com.starship.ios.achievement.barrel_roll";
NSString * const kAchievementWingmanSaver = @"com.starship.ios.achievement.wingman_saver";

static NSString * const kGameCenterEnabledKey = @"gameCenterEnabled";

@interface GameCenterManager ()
@property (nonatomic, readwrite) BOOL isAuthenticated;
@property (nonatomic, readwrite) BOOL isEnabled;
@end

@implementation GameCenterManager

+ (instancetype)sharedManager {
    static GameCenterManager *sharedManager = nil;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        sharedManager = [[GameCenterManager alloc] init];
    });
    return sharedManager;
}

- (instancetype)init {
    self = [super init];
    if (self) {
        _isAuthenticated = NO;
        // Default to enabled, user can disable in settings
        _isEnabled = [[NSUserDefaults standardUserDefaults] objectForKey:kGameCenterEnabledKey] == nil
            ? YES
            : [[NSUserDefaults standardUserDefaults] boolForKey:kGameCenterEnabledKey];
    }
    return self;
}

- (void)setEnabled:(BOOL)enabled {
    _isEnabled = enabled;
    [[NSUserDefaults standardUserDefaults] setBool:enabled forKey:kGameCenterEnabledKey];
    [[NSUserDefaults standardUserDefaults] synchronize];

    if (enabled && !self.isAuthenticated) {
        [self authenticateWithCompletion:nil];
    }

    NSLog(@"[GameCenter] Enabled: %@", enabled ? @"YES" : @"NO");
}

#pragma mark - Authentication

- (void)authenticateWithCompletion:(void(^)(BOOL success, NSError *error))completion {
    if (!self.isEnabled) {
        NSLog(@"[GameCenter] Skipping authentication - Game Center is disabled");
        if (completion) {
            completion(NO, nil);
        }
        return;
    }

    GKLocalPlayer *localPlayer = [GKLocalPlayer localPlayer];

    localPlayer.authenticateHandler = ^(UIViewController * _Nullable viewController, NSError * _Nullable error) {
        if (viewController) {
            // Present the Game Center sign-in view controller
            UIViewController *rootVC = [self topViewController];
            if (rootVC) {
                [rootVC presentViewController:viewController animated:YES completion:nil];
            }
        } else if (localPlayer.isAuthenticated) {
            self.isAuthenticated = YES;
            self.localPlayer = localPlayer;
            NSLog(@"[GameCenter] Authenticated as: %@", localPlayer.displayName);

            if (completion) {
                completion(YES, nil);
            }
        } else {
            self.isAuthenticated = NO;
            self.localPlayer = nil;

            if (error) {
                NSLog(@"[GameCenter] Authentication failed: %@", error.localizedDescription);
            } else {
                NSLog(@"[GameCenter] Player is not authenticated");
            }

            if (completion) {
                completion(NO, error);
            }
        }
    };
}

#pragma mark - Leaderboards

- (void)submitScore:(int64_t)score toLeaderboard:(NSString *)leaderboardID {
    [self submitScore:score toLeaderboard:leaderboardID withCompletion:nil];
}

- (void)submitScore:(int64_t)score toLeaderboard:(NSString *)leaderboardID withCompletion:(void(^)(BOOL success, NSError *error))completion {
    if (!self.isEnabled || !self.isAuthenticated) {
        NSLog(@"[GameCenter] Cannot submit score - not enabled or authenticated");
        if (completion) {
            completion(NO, nil);
        }
        return;
    }

    [GKLeaderboard submitScore:score
                       context:0
                        player:self.localPlayer
           leaderboardIDs:@[leaderboardID]
        completionHandler:^(NSError * _Nullable error) {
            if (error) {
                NSLog(@"[GameCenter] Failed to submit score: %@", error.localizedDescription);
                if (completion) {
                    completion(NO, error);
                }
            } else {
                NSLog(@"[GameCenter] Score %lld submitted to %@", score, leaderboardID);
                if (completion) {
                    completion(YES, nil);
                }
            }
        }];
}

#pragma mark - Achievements

- (void)unlockAchievement:(NSString *)achievementID {
    [self unlockAchievement:achievementID withCompletion:nil];
}

- (void)unlockAchievement:(NSString *)achievementID withCompletion:(void(^)(BOOL success, NSError *error))completion {
    [self reportAchievementProgress:achievementID percentComplete:100.0 withCompletion:completion];
}

- (void)reportAchievementProgress:(NSString *)achievementID percentComplete:(double)percent {
    [self reportAchievementProgress:achievementID percentComplete:percent withCompletion:nil];
}

- (void)reportAchievementProgress:(NSString *)achievementID percentComplete:(double)percent withCompletion:(void(^)(BOOL success, NSError *error))completion {
    if (!self.isEnabled || !self.isAuthenticated) {
        NSLog(@"[GameCenter] Cannot report achievement - not enabled or authenticated");
        if (completion) {
            completion(NO, nil);
        }
        return;
    }

    GKAchievement *achievement = [[GKAchievement alloc] initWithIdentifier:achievementID];
    achievement.percentComplete = percent;
    achievement.showsCompletionBanner = YES;

    [GKAchievement reportAchievements:@[achievement] withCompletionHandler:^(NSError * _Nullable error) {
        if (error) {
            NSLog(@"[GameCenter] Failed to report achievement: %@", error.localizedDescription);
            if (completion) {
                completion(NO, error);
            }
        } else {
            NSLog(@"[GameCenter] Achievement %@ reported at %.0f%%", achievementID, percent);
            if (completion) {
                completion(YES, nil);
            }
        }
    }];
}

#pragma mark - UI

- (UIViewController *)topViewController {
    UIWindowScene *windowScene = nil;

    for (UIScene *scene in [UIApplication sharedApplication].connectedScenes) {
        if ([scene isKindOfClass:[UIWindowScene class]]) {
            windowScene = (UIWindowScene *)scene;
            break;
        }
    }

    if (!windowScene) return nil;

    UIViewController *rootVC = windowScene.windows.firstObject.rootViewController;
    while (rootVC.presentedViewController) {
        rootVC = rootVC.presentedViewController;
    }
    return rootVC;
}

- (void)showLeaderboards {
    [self showGameCenterViewControllerWithState:GKGameCenterViewControllerStateLeaderboards];
}

- (void)showLeaderboard:(NSString *)leaderboardID {
    if (!self.isEnabled || !self.isAuthenticated) {
        NSLog(@"[GameCenter] Cannot show leaderboard - not enabled or authenticated");
        return;
    }

    GKGameCenterViewController *gcVC = [[GKGameCenterViewController alloc] initWithLeaderboardID:leaderboardID
                                                                                    playerScope:GKLeaderboardPlayerScopeGlobal
                                                                                       timeScope:GKLeaderboardTimeScopeAllTime];
    gcVC.gameCenterDelegate = (id<GKGameCenterControllerDelegate>)self;

    UIViewController *rootVC = [self topViewController];
    if (rootVC) {
        [rootVC presentViewController:gcVC animated:YES completion:nil];
    }
}

- (void)showAchievements {
    [self showGameCenterViewControllerWithState:GKGameCenterViewControllerStateAchievements];
}

- (void)showGameCenterDashboard {
    [self showGameCenterViewControllerWithState:GKGameCenterViewControllerStateDefault];
}

- (void)showGameCenterViewControllerWithState:(GKGameCenterViewControllerState)state {
    if (!self.isEnabled || !self.isAuthenticated) {
        NSLog(@"[GameCenter] Cannot show Game Center - not enabled or authenticated");
        return;
    }

    GKGameCenterViewController *gcVC = [[GKGameCenterViewController alloc] initWithState:state];
    gcVC.gameCenterDelegate = (id<GKGameCenterControllerDelegate>)self;

    UIViewController *rootVC = [self topViewController];
    if (rootVC) {
        [rootVC presentViewController:gcVC animated:YES completion:nil];
    }
}

#pragma mark - GKGameCenterControllerDelegate

- (void)gameCenterViewControllerDidFinish:(GKGameCenterViewController *)gameCenterViewController {
    [gameCenterViewController dismissViewControllerAnimated:YES completion:nil];
}

@end
