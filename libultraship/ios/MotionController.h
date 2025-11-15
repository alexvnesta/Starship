//
//  MotionController.h
//  Starship
//
//  Handles device motion/gyroscope input for Star Fox 64 flight controls
//

#import <Foundation/Foundation.h>
#import <CoreMotion/CoreMotion.h>

NS_ASSUME_NONNULL_BEGIN

// Gyro control modes (for future feature toggle)
typedef NS_ENUM(NSInteger, MotionControlMode) {
    MotionControlModeAttitude,      // Attitude-based: tilt angle → stick position (for assist mode)
    MotionControlModeIntegrated     // Integration-based: rotation rate → mouse-like movement (future feature)
};

@interface MotionController : NSObject

@property (nonatomic, assign) BOOL enabled;
@property (nonatomic, assign) CGFloat sensitivity;  // For attitude mode: degrees of tilt for full deflection (default 20.0)
@property (nonatomic, assign) BOOL invertPitch;
@property (nonatomic, assign) BOOL invertRoll;
@property (nonatomic, assign) CGFloat deadzone;    // Deadzone in degrees (default 0.5, small for progressive feel)
@property (nonatomic, assign) CGFloat responseCurve;  // Response curve exponent: 1.0=linear, 2.0=squared (progressive), 3.0=cubed (default 2.0)
@property (nonatomic, assign) MotionControlMode mode;  // Current control mode (default: Attitude)

// Singleton instance
+ (instancetype)sharedController;

// Lifecycle
- (void)startMotionUpdates;
- (void)stopMotionUpdates;

// Recalibration
- (void)recalibrate;

// Load settings from CVars
- (void)loadSettingsFromCVars;

// Get current axis values (-1.0 to 1.0)
- (CGFloat)axisX;  // Horizontal (left/right)
- (CGFloat)axisY;  // Vertical (up/down)

// Get relative pitch angle in degrees (for barrel roll detection)
- (double)relativePitchDegrees;

// Debug accessors for raw rotation rate (rad/s)
- (double)rawRotationRateX;
- (double)rawRotationRateY;
- (double)rawRotationRateZ;

@end

NS_ASSUME_NONNULL_END
