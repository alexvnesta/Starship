//
//  ViewIntegration.mm
//  Starship iOS
//
//  Integration between SDL's rendering view and iOS touch controls
//  This ensures touch controls appear on top of the game rendering
//

#import <UIKit/UIKit.h>
#import <SDL.h>
#import <SDL_syswm.h>
#import "StarshipViewController.h"
#import "StarshipBridge.h"

// Global reference to the view controller
static StarshipViewController* g_viewController = nil;

void iOS_IntegrateSDLView(void* sdlWindow) {
    if (!sdlWindow) {
        NSLog(@"[iOS View Integration] Error: SDL window is null");
        return;
    }

    NSLog(@"[iOS View Integration] Starting integration...");

    SDL_Window* window = (SDL_Window*)sdlWindow;

    // Get the SDL window's UIWindow
    SDL_SysWMinfo wmInfo;
    SDL_VERSION(&wmInfo.version);
    if (!SDL_GetWindowWMInfo(window, &wmInfo)) {
        NSLog(@"[iOS View Integration] Error: Failed to get window info: %s", SDL_GetError());
        return;
    }

    UIWindow* uiWindow = wmInfo.info.uikit.window;
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

    // Ensure our view controller's view is loaded
    [g_viewController loadViewIfNeeded];

    // Set the root view controller for SDL's window
    uiWindow.rootViewController = g_viewController;
    [uiWindow makeKeyAndVisible];

    NSLog(@"[iOS View Integration] SDL window root view controller set");

    // Create a SEPARATE overlay window for touch controls that sits on top of SDL
    // This ensures SDL's rendering cannot block the touch controls
    g_viewController.overlayWindow = [[UIWindow alloc] initWithFrame:uiWindow.bounds];
    g_viewController.overlayWindow.windowLevel = UIWindowLevelNormal + 1;  // Above normal windows
    g_viewController.overlayWindow.backgroundColor = [UIColor clearColor];
    g_viewController.overlayWindow.userInteractionEnabled = YES;
    g_viewController.overlayWindow.opaque = NO;

    // Create a transparent view controller for the overlay window
    UIViewController *overlayVC = [[UIViewController alloc] init];
    overlayVC.view.backgroundColor = [UIColor clearColor];
    overlayVC.view.frame = g_viewController.overlayWindow.bounds;
    g_viewController.overlayWindow.rootViewController = overlayVC;

    // Add touch controls to the overlay window
    g_viewController.touchControlsContainer.frame = overlayVC.view.bounds;
    g_viewController.touchControlsContainer.hidden = NO;
    g_viewController.touchControlsContainer.alpha = 1.0;
    g_viewController.touchControlsContainer.userInteractionEnabled = YES;
    [overlayVC.view addSubview:g_viewController.touchControlsContainer];

    // Make the overlay window visible
    [g_viewController.overlayWindow makeKeyAndVisible];

    NSLog(@"[iOS View Integration] Created separate overlay window for touch controls");
    NSLog(@"[iOS View Integration] Overlay window level: %f", g_viewController.overlayWindow.windowLevel);
    NSLog(@"[iOS View Integration] Touch controls container frame: %@, hidden: %d, alpha: %.2f",
          NSStringFromCGRect(g_viewController.touchControlsContainer.frame),
          g_viewController.touchControlsContainer.hidden,
          g_viewController.touchControlsContainer.alpha);

    NSLog(@"[iOS View Integration] Integration complete! Touch controls should now be visible.");
}
