//
//  StarshipViewController.h
//  Starship iOS
//
//  View controller for iOS touch controls and game integration
//

#import <UIKit/UIKit.h>

NS_ASSUME_NONNULL_BEGIN

/**
 * Main view controller for Starship on iOS
 * Manages touch controls overlay and integrates with SDL2 game loop
 */
@interface StarshipViewController : UIViewController

/**
 * Container view for all touch controls
 */
@property (nonatomic, readonly) UIView *touchControlsContainer;

/**
 * Separate window for touch controls overlay (sits on top of SDL window)
 */
@property (nonatomic, strong) UIWindow *overlayWindow;

/**
 * Toggle visibility of on-screen touch controls
 * @param visible YES to show controls, NO to hide (e.g., when physical controller connected)
 */
- (void)setTouchControlsVisible:(BOOL)visible;

/**
 * Check if touch controls are currently being used
 * @return YES if touch controls are active, NO if using physical controller
 */
- (BOOL)isUsingTouchControls;

@end

NS_ASSUME_NONNULL_END
