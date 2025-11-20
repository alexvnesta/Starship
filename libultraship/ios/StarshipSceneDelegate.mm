#import "StarshipSceneDelegate.h"

@implementation StarshipSceneDelegate

- (void)scene:(UIScene *)scene willConnectToSession:(UISceneSession *)session options:(UISceneConnectionOptions *)connectionOptions {
    // SDL creates its own window at runtime
    // We need to find SDL's window and assign it to self.window so it becomes the key window
    if ([scene isKindOfClass:[UIWindowScene class]]) {
        UIWindowScene *windowScene = (UIWindowScene *)scene;

        // SDL creates its window before this is called, so find it
        // Iterate through all windows in the window scene to find SDL's window
        for (UIWindow *window in windowScene.windows) {
            if ([NSStringFromClass([window class]) containsString:@"SDL"]) {
                // Found SDL's window - assign it to self.window
                self.window = window;
                NSLog(@"[SceneDelegate] Found and assigned SDL window: %@", window);

                // Make it key and visible through the scene delegate
                self.window.windowScene = windowScene;
                [self.window makeKeyAndVisible];
                break;
            }
        }

        // If we haven't found the window yet, SDL might create it later
        // Set up a small delay to check again
        if (!self.window) {
            NSLog(@"[SceneDelegate] SDL window not found yet, will retry");
            dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(0.1 * NSEC_PER_SEC)), dispatch_get_main_queue(), ^{
                for (UIWindow *window in windowScene.windows) {
                    if ([NSStringFromClass([window class]) containsString:@"SDL"]) {
                        self.window = window;
                        NSLog(@"[SceneDelegate] Found SDL window on retry: %@", window);
                        self.window.windowScene = windowScene;
                        [self.window makeKeyAndVisible];
                        break;
                    }
                }
            });
        }
    }
}

- (void)sceneDidDisconnect:(UIScene *)scene {
    // Called when the scene is being released by the system
}

- (void)sceneDidBecomeActive:(UIScene *)scene {
    // Called when the scene has moved from an inactive state to an active state
}

- (void)sceneWillResignActive:(UIScene *)scene {
    // Called when the scene will move from an active state to an inactive state
}

- (void)sceneWillEnterForeground:(UIScene *)scene {
    // Called as the scene transitions from the background to the foreground
}

- (void)sceneDidEnterBackground:(UIScene *)scene {
    // Called as the scene transitions from the foreground to the background
}

@end
