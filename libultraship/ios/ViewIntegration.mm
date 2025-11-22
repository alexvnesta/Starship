//
//  ViewIntegration.mm
//  Starship iOS
//
//  Integration between SDL's rendering view and iOS touch controls
//  NEW APPROACH: Add touch controls as a subview of SDL's view hierarchy
//  This eliminates window management issues - all touches go to the same window
//

#import <UIKit/UIKit.h>
#import <SDL3/SDL.h>
#import "StarshipViewController.h"
#import "StarshipBridge.h"
#import "StarshipSceneDelegate.h"

// Global reference to the view controller
static StarshipViewController* g_viewController = nil;

// Global reference to SDL's UIWindow
static UIWindow* g_sdlWindow = nil;

// Global reference to touch controls container view
static UIView* g_touchControlsView = nil;

// Pending menu state to apply when touch controls are created
static bool g_pendingMenuOpenState = false;
static bool g_hasPendingMenuState = false;

void iOS_IntegrateSDLView(void* sdlWindow) {
    if (!sdlWindow) {
        NSLog(@"[iOS View Integration] Error: SDL window is null");
        return;
    }

    NSLog(@"[iOS View Integration] Starting integration (single window approach)...");

    SDL_Window* window = (SDL_Window*)sdlWindow;

    // Get the SDL window's UIWindow using SDL3 properties API
    UIWindow* uiWindow = (__bridge UIWindow*)SDL_GetPointerProperty(SDL_GetWindowProperties(window), SDL_PROP_WINDOW_UIKIT_WINDOW_POINTER, NULL);
    if (!uiWindow) {
        NSLog(@"[iOS View Integration] Error: UIWindow is null");
        return;
    }

    NSLog(@"[iOS View Integration] Got UIWindow: %@", uiWindow);
    g_sdlWindow = uiWindow;

    // Create the StarshipViewController if needed
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

    // CRITICAL: Set the SceneDelegate's window property for iOS 13+ scene-based lifecycle
    if (uiWindow.windowScene) {
        UIWindowScene *windowScene = uiWindow.windowScene;
        if (windowScene.delegate && [windowScene.delegate isKindOfClass:[StarshipSceneDelegate class]]) {
            StarshipSceneDelegate *sceneDelegate = (StarshipSceneDelegate *)windowScene.delegate;
            sceneDelegate.window = uiWindow;
            NSLog(@"[iOS View Integration] ✅ Set SceneDelegate.window property");
        }
    } else {
        // Try to find and associate with active window scene
        NSSet<UIScene *> *connectedScenes = [UIApplication sharedApplication].connectedScenes;
        for (UIScene *scene in connectedScenes) {
            if ([scene isKindOfClass:[UIWindowScene class]]) {
                UIWindowScene *windowScene = (UIWindowScene *)scene;
                uiWindow.windowScene = windowScene;
                if (windowScene.delegate && [windowScene.delegate isKindOfClass:[StarshipSceneDelegate class]]) {
                    StarshipSceneDelegate *sceneDelegate = (StarshipSceneDelegate *)windowScene.delegate;
                    sceneDelegate.window = uiWindow;
                    NSLog(@"[iOS View Integration] ✅ Set SceneDelegate.window after manual association");
                }
                break;
            }
        }
    }

    // Make sure the window is visible and key
    uiWindow.hidden = NO;
    uiWindow.alpha = 1.0;
    [uiWindow makeKeyAndVisible];

    NSLog(@"[iOS View Integration] Made SDL window key and visible");

    // Load the view controller's view to trigger viewDidLoad and setupTouchControls
    [g_viewController loadViewIfNeeded];

    // Get the touch controls container from our view controller
    UIView* touchControls = g_viewController.touchControlsContainer;
    if (touchControls) {
        // Remove from current parent if any
        [touchControls removeFromSuperview];

        // Add touch controls as a subview of SDL's view (on top)
        touchControls.frame = sdlView.bounds;
        touchControls.autoresizingMask = UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight;
        touchControls.backgroundColor = [UIColor clearColor];
        touchControls.opaque = NO;
        touchControls.userInteractionEnabled = YES;

        NSLog(@"[iOS View Integration] 🔧 Touch controls initial state: hidden=%d, userInteraction=%d",
              touchControls.hidden, touchControls.userInteractionEnabled);

        [sdlView addSubview:touchControls];
        [sdlView bringSubviewToFront:touchControls];

        g_touchControlsView = touchControls;

        NSLog(@"[iOS View Integration] ✅ Added touch controls as subview of SDL view");
        NSLog(@"[iOS View Integration] Touch controls frame: %@", NSStringFromCGRect(touchControls.frame));
        NSLog(@"[iOS View Integration] SDL view subviews: %@", sdlView.subviews);

        // Apply any pending menu state that was set before touch controls were created
        NSLog(@"[iOS View Integration] 🔍 Checking pending state: g_hasPendingMenuState=%d, g_pendingMenuOpenState=%d",
              g_hasPendingMenuState, g_pendingMenuOpenState);

        if (g_hasPendingMenuState) {
            NSLog(@"[iOS View Integration] 🟡 Applying pending menu state: menuOpen=%d", g_pendingMenuOpenState);
            touchControls.hidden = g_pendingMenuOpenState;
            touchControls.userInteractionEnabled = !g_pendingMenuOpenState;
            NSLog(@"[iOS View Integration]    Result: hidden=%d, userInteraction=%d",
                  touchControls.hidden, touchControls.userInteractionEnabled);
            g_hasPendingMenuState = false;
        } else {
            NSLog(@"[iOS View Integration] ℹ️ No pending state to apply");
        }

        NSLog(@"[iOS View Integration] 📊 Final touch controls state: hidden=%d, userInteraction=%d",
              touchControls.hidden, touchControls.userInteractionEnabled);
    } else {
        NSLog(@"[iOS View Integration] ❌ Error: Touch controls container is null");
    }

    // Store reference for the view controller (for compatibility)
    g_viewController.overlayWindow = uiWindow;

    NSLog(@"[iOS View Integration] Integration complete (single window mode)");
}

void iOS_SetMenuOpen(bool menuOpen) {
    // Single window approach: touch controls are a subview of SDL's view
    // When menu opens: hide touch controls so ImGui can receive touches
    // When menu closes: show touch controls again

    NSLog(@"[iOS Menu] 🔵 iOS_SetMenuOpen called with menuOpen=%d", menuOpen);
    NSLog(@"[iOS Menu]    Before: g_pendingMenuOpenState=%d, g_hasPendingMenuState=%d",
          g_pendingMenuOpenState, g_hasPendingMenuState);
    NSLog(@"[iOS Menu]    g_touchControlsView exists: %d", g_touchControlsView != nil);

    // Always store the desired state
    g_pendingMenuOpenState = menuOpen;
    g_hasPendingMenuState = true;

    dispatch_async(dispatch_get_main_queue(), ^{
        // Read the latest pending state (don't capture menuOpen parameter)
        bool latestMenuOpenState = g_pendingMenuOpenState;

        NSLog(@"[iOS Menu] 🟢 Async block running, g_touchControlsView exists: %d", g_touchControlsView != nil);
        NSLog(@"[iOS Menu]    Applying state: latestMenuOpenState=%d", latestMenuOpenState);

        if (g_touchControlsView) {
            // Simply hide/show the touch controls subview
            // When hidden, touches pass through to SDL/ImGui automatically
            g_touchControlsView.hidden = latestMenuOpenState;
            g_touchControlsView.userInteractionEnabled = !latestMenuOpenState;

            NSLog(@"[iOS Menu] ✅ Applied state: hidden=%d, userInteraction=%d",
                  g_touchControlsView.hidden, g_touchControlsView.userInteractionEnabled);

            // Clear pending state since we applied it
            g_hasPendingMenuState = false;
            NSLog(@"[iOS Menu]    Cleared pending state flag");
        } else {
            NSLog(@"[iOS Menu] ⏳ Touch controls not yet created, keeping pending state: menuOpen=%d", latestMenuOpenState);
        }

        // Also update the view controller's container reference if it exists
        if (g_viewController.touchControlsContainer) {
            g_viewController.touchControlsContainer.hidden = latestMenuOpenState;
            g_viewController.touchControlsContainer.userInteractionEnabled = !latestMenuOpenState;
        }
    });
}
