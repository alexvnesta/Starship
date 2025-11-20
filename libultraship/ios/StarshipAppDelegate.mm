#import "StarshipAppDelegate.h"

@implementation StarshipAppDelegate

- (BOOL)application:(UIApplication *)application didFinishLaunchingWithOptions:(NSDictionary *)launchOptions {
    // SDL will create its own window, so we don't create one here
    // Note: Audio session is configured via +load method in AudioSessionHelper.mm (before main() is called)
    return YES;
}

#pragma mark - UISceneSession Lifecycle

- (UISceneConfiguration *)application:(UIApplication *)application
    configurationForConnectingSceneSession:(UISceneSession *)connectingSceneSession
                                   options:(UISceneConnectionOptions *)options {
    // Called when a new scene session is being created
    // Use this method to select a configuration to create the new scene with
    UISceneConfiguration *configuration = [[UISceneConfiguration alloc] initWithName:@"Default Configuration"
                                                                         sessionRole:connectingSceneSession.role];
    configuration.delegateClass = NSClassFromString(@"StarshipSceneDelegate");
    return configuration;
}

- (void)application:(UIApplication *)application
didDiscardSceneSessions:(NSSet<UISceneSession *> *)sceneSessions {
    // Called when the user discards a scene session
    // Use this method to release any resources that were specific to the discarded scenes
}

@end
