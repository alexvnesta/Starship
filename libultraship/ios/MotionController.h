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
@property (nonatomic, assign) CGFloat deadzone;    // Deadzone in degrees (default 2.0)
@property (nonatomic, assign) MotionControlMode mode;  // Current control mode (default: Attitude)

// Singleton instance
+ (instancetype)sharedController;

// Lifecycle
- (void)startMotionUpdates;
- (void)stopMotionUpdates;

// Recalibration
- (void)recalibrate;

// Get current axis values (-1.0 to 1.0)
- (CGFloat)axisX;  // Roll (left/right)
- (CGFloat)axisY;  // Pitch (up/down)

// Debug accessors for raw rotation rate (rad/s)
- (double)rawRotationRateX;
- (double)rawRotationRateY;
- (double)rawRotationRateZ;

@end

NS_ASSUME_NONNULL_END
