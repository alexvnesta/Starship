//
//  ViewIntegration.mm
//  Starship iOS
//
//  Integration between SDL's rendering view and iOS touch controls
//  This ensures touch controls appear on top of the game rendering
//

#import <UIKit/UIKit.h>
#import <SDL3/SDL.h>
#import "StarshipViewController.h"
#import "StarshipBridge.h"
#import "StarshipSceneDelegate.h"

// Global reference to the view controller
static StarshipViewController* g_viewController = nil;

// Global reference to the SDL UIWindow for menu focus switching
static UIWindow* g_sdlWindow = nil;

void iOS_IntegrateSDLView(void* sdlWindow) {
    if (!sdlWindow) {
        NSLog(@"[iOS View Integration] Error: SDL window is null");
        return;
    }

    NSLog(@"[iOS View Integration] Starting integration...");

    SDL_Window* window = (SDL_Window*)sdlWindow;

    // Get the SDL window's UIWindow using SDL3 properties API
    UIWindow* uiWindow = (__bridge UIWindow*)SDL_GetPointerProperty(SDL_GetWindowProperties(window), SDL_PROP_WINDOW_UIKIT_WINDOW_POINTER, NULL);
    if (!uiWindow) {
        NSLog(@"[iOS View Integration] Error: UIWindow is null");
        return;
    }

    NSLog(@"[iOS View Integration] Got UIWindow: %@", uiWindow);

    // Create or get the StarshipViewController
    if (!g_viewController) {
        g_viewController = [[StarshipViewController alloc] init];
        NSLog(@"[iOS View Integration] Created StarshipViewController");
    }

    // Get SDL's root view controller and its view
    UIViewController* sdlViewController = uiWindow.rootViewController;
    if (!sdlViewController) {
        NSLog(@"[iOS View Integration] Error: SDL root view controller is null");
        return;
    }

    UIView* sdlView = sdlViewController.view;
    if (!sdlView) {
        NSLog(@"[iOS View Integration] Error: SDL view is null");
        return;
    }

    NSLog(@"[iOS View Integration] Got SDL view: %@", sdlView);
    NSLog(@"[iOS View Integration] SDL view frame: %@", NSStringFromCGRect(sdlView.frame));
    NSLog(@"[iOS View Integration] SDL view bounds: %@", NSStringFromCGRect(sdlView.bounds));
    NSLog(@"[iOS View Integration] SDL view controller: %@", sdlViewController);
    NSLog(@"[iOS View Integration] SDL view layer: %@", sdlView.layer);
    NSLog(@"[iOS View Integration] SDL view layer class: %@", NSStringFromClass([sdlView.layer class]));
    NSLog(@"[iOS View Integration] SDL view backgroundColor: %@", sdlView.backgroundColor);
    NSLog(@"[iOS View Integration] SDL view opaque: %d", sdlView.opaque);
    NSLog(@"[iOS View Integration] SDL view hidden: %d", sdlView.hidden);
    NSLog(@"[iOS View Integration] SDL view alpha: %.2f", sdlView.alpha);
    NSLog(@"[iOS View Integration] SDL view superview: %@", sdlView.superview);
    NSLog(@"[iOS View Integration] SDL window: %@", uiWindow);
    NSLog(@"[iOS View Integration] SDL window hidden: %d", uiWindow.hidden);
    NSLog(@"[iOS View Integration] SDL window alpha: %.2f", uiWindow.alpha);
    NSLog(@"[iOS View Integration] SDL window isKeyWindow: %d", uiWindow.isKeyWindow);

    // CRITICAL: Set the SceneDelegate's window property for iOS 13+ scene-based lifecycle
    // The SceneDelegate MUST own the window for it to become key and visible
    NSLog(@"[iOS View Integration] Checking window scene...");
    NSLog(@"[iOS View Integration] uiWindow.windowScene: %@", uiWindow.windowScene);

    if (uiWindow.windowScene) {
        UIWindowScene *windowScene = uiWindow.windowScene;
        NSLog(@"[iOS View Integration] windowScene.delegate: %@", windowScene.delegate);
        NSLog(@"[iOS View Integration] windowScene.delegate class: %@", NSStringFromClass([windowScene.delegate class]));

        if (windowScene.delegate && [windowScene.delegate isKindOfClass:[StarshipSceneDelegate class]]) {
            StarshipSceneDelegate *sceneDelegate = (StarshipSceneDelegate *)windowScene.delegate;
            sceneDelegate.window = uiWindow;
            NSLog(@"[iOS View Integration] ✅ Set SceneDelegate.window property - window will now become key");
        } else {
            NSLog(@"[iOS View Integration] ❌ Warning: SceneDelegate is not StarshipSceneDelegate class");
        }
    } else {
        NSLog(@"[iOS View Integration] ❌ ERROR: uiWindow.windowScene is NIL - SDL window not associated with scene!");
        NSLog(@"[iOS View Integration] This is the root cause - need to associate window with scene manually");

        // Try to find the active window scene and associate the window with it
        NSSet<UIScene *> *connectedScenes = [UIApplication sharedApplication].connectedScenes;
        NSLog(@"[iOS View Integration] Connected scenes: %@", connectedScenes);

        for (UIScene *scene in connectedScenes) {
            if ([scene isKindOfClass:[UIWindowScene class]]) {
                UIWindowScene *windowScene = (UIWindowScene *)scene;
                NSLog(@"[iOS View Integration] Found UIWindowScene: %@", windowScene);

                // Associate the window with this scene
                uiWindow.windowScene = windowScene;
                NSLog(@"[iOS View Integration] Associated SDL window with scene");

                // Now try to set the delegate's window property
                if (windowScene.delegate && [windowScene.delegate isKindOfClass:[StarshipSceneDelegate class]]) {
                    StarshipSceneDelegate *sceneDelegate = (StarshipSceneDelegate *)windowScene.delegate;
                    sceneDelegate.window = uiWindow;
                    NSLog(@"[iOS View Integration] ✅ Set SceneDelegate.window property after manual scene association");
                }
                break;
            }
        }
    }

    // Store reference to SDL window for menu focus switching
    g_sdlWindow = uiWindow;

    // Make sure the window is visible and key
    uiWindow.hidden = NO;
    uiWindow.alpha = 1.0;
    [uiWindow makeKeyAndVisible];

    // Force the Metal layer to display
    CAMetalLayer* metalLayer = (CAMetalLayer*)sdlView.layer;
    [metalLayer setNeedsDisplay];
    [metalLayer displayIfNeeded];

    // Force the view and window to update
    [sdlView setNeedsDisplay];
    [sdlView setNeedsLayout];
    [sdlView layoutIfNeeded];
    [uiWindow setNeedsDisplay];
    [uiWindow setNeedsLayout];
    [uiWindow layoutIfNeeded];

    NSLog(@"[iOS View Integration] Made window key and visible");
    NSLog(@"[iOS View Integration] Forced display updates on view and layer");

    // Create overlay window for touch controls
    // This window sits on top of the SDL window to display touch controls
    UIWindowScene *windowScene = uiWindow.windowScene;
    if (windowScene) {
        // Load the view controller's view to trigger viewDidLoad and setupTouchControls
        [g_viewController loadViewIfNeeded];

        // Create a new UIWindow for the touch controls overlay
        UIWindow *overlayWindow = [[UIWindow alloc] initWithWindowScene:windowScene];
        overlayWindow.rootViewController = g_viewController;
        overlayWindow.windowLevel = UIWindowLevelNormal + 1;  // Above SDL window
        overlayWindow.backgroundColor = [UIColor clearColor];  // Transparent background
        overlayWindow.opaque = NO;
        overlayWindow.userInteractionEnabled = YES;

        // Make overlay window visible AND key so it can receive touch events
        // Metal rendering works fine even if SDL window is not key
        [overlayWindow makeKeyAndVisible];

        // Store reference in view controller
        g_viewController.overlayWindow = overlayWindow;

        NSLog(@"[iOS View Integration] Created touch controls overlay window");
        NSLog(@"[iOS View Integration] Overlay window: %@", overlayWindow);
        NSLog(@"[iOS View Integration] Touch controls container: %@", g_viewController.touchControlsContainer);
        NSLog(@"[iOS View Integration] Touch controls hidden: %d", g_viewController.touchControlsContainer.hidden);
        NSLog(@"[iOS View Integration] Overlay window isKeyWindow: %d", overlayWindow.isKeyWindow);
        NSLog(@"[iOS View Integration] SDL window isKeyWindow: %d", uiWindow.isKeyWindow);
    } else {
        NSLog(@"[iOS View Integration] ❌ ERROR: Cannot create overlay window - no window scene available");
    }

    NSLog(@"[iOS View Integration] Integration complete");
}

// Called when menu visibility changes - switches which window receives touch input
// Menu OPEN: SDL window becomes key so ImGui can receive touch events
// Menu CLOSED: Overlay window becomes key so game controls work
extern "C" void iOS_SetMenuOpen(bool menuOpen) {
    NSLog(@"[iOS View Integration] iOS_SetMenuOpen called with menuOpen=%d", menuOpen);

    if (!g_sdlWindow || !g_viewController || !g_viewController.overlayWindow) {
        NSLog(@"[iOS View Integration] Warning: Windows not initialized yet, cannot switch focus");
        return;
    }

    UIWindow* overlayWindow = g_viewController.overlayWindow;

    dispatch_async(dispatch_get_main_queue(), ^{
        if (menuOpen) {
            // Menu opening: SDL window needs to receive touches for ImGui
            NSLog(@"[iOS View Integration] Menu OPEN - switching to SDL window for ImGui touch input");

            // Disable touch interaction on overlay so touches pass through to SDL window
            overlayWindow.userInteractionEnabled = NO;

            // Make SDL window key so it receives touch events
            [g_sdlWindow makeKeyAndVisible];

            // Hide touch controls while menu is open
            g_viewController.touchControlsContainer.hidden = YES;

            NSLog(@"[iOS View Integration] SDL window isKeyWindow: %d, overlay userInteraction: %d",
                  g_sdlWindow.isKeyWindow, overlayWindow.userInteractionEnabled);
        } else {
            // Menu closing: Overlay needs to receive touches for game controls
            NSLog(@"[iOS View Integration] Menu CLOSED - switching to overlay window for game controls");

            // Re-enable touch interaction on overlay
            overlayWindow.userInteractionEnabled = YES;

            // Make overlay window key so it receives touch events for game controls
            [overlayWindow makeKeyAndVisible];

            // Show touch controls
            g_viewController.touchControlsContainer.hidden = NO;

            NSLog(@"[iOS View Integration] Overlay window isKeyWindow: %d, userInteraction: %d",
                  overlayWindow.isKeyWindow, overlayWindow.userInteractionEnabled);
        }
    });
}
