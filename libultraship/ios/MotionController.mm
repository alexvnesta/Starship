//
//  MotionController.mm
//  Starship
//
//  Handles device motion/gyroscope input for Star Fox 64 flight controls
//

#import "MotionController.h"
#import <UIKit/UIKit.h>

// Conversion factor: radians to degrees
#define RAD_TO_DEG (180.0 / M_PI)

// Update frequency in Hz
#define UPDATE_FREQUENCY 60.0

@interface MotionController ()
@property (nonatomic, strong) CMMotionManager *motionManager;
@property (nonatomic, assign) CGFloat currentAxisX;     // Current X axis value (attitude or integrated)
@property (nonatomic, assign) CGFloat currentAxisY;     // Current Y axis value (attitude or integrated)
@property (nonatomic, assign) double currentRelativePitch; // Current relative pitch in radians (for barrel roll)
@property (nonatomic, assign) double rawRotX;           // Raw rotation rate X (for debugging)
@property (nonatomic, assign) double rawRotY;           // Raw rotation rate Y (for debugging)
@property (nonatomic, assign) double rawRotZ;           // Raw rotation rate Z (for debugging)

// Attitude mode properties
@property (nonatomic, assign) double referenceRoll;     // Reference roll angle for zeroing
@property (nonatomic, assign) double referenceYaw;      // Reference yaw angle for zeroing
@property (nonatomic, assign) double referencePitch;    // Reference pitch angle for zeroing

// Integration mode properties (for future use)
@property (nonatomic, strong) NSDate *lastUpdateTime;   // For calculating delta time
@property (nonatomic, assign) double gyroCalibrationX;  // Manual calibration offset
@property (nonatomic, assign) double gyroCalibrationY;  // Manual calibration offset
@end

@implementation MotionController

+ (instancetype)sharedController {
    static MotionController *sharedInstance = nil;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        sharedInstance = [[self alloc] init];
    });
    return sharedInstance;
}

- (instancetype)init {
    self = [super init];
    if (self) {
        // Initialize motion manager
        _motionManager = [[CMMotionManager alloc] init];
        _motionManager.deviceMotionUpdateInterval = 1.0 / UPDATE_FREQUENCY;

        // Default settings for attitude-based mode
        _mode = MotionControlModeAttitude;
        _enabled = YES;
        _sensitivity = 20.0;     // Degrees of tilt for full deflection (±20° = ±1.0)
        _invertPitch = YES;      // Inverted for correct pitch direction in landscape mode
        _invertRoll = YES;       // Inverted to match original working behavior
        _deadzone = 0.5;         // Small deadzone (0.5 degrees) for progressive feel
        _responseCurve = 2.0;    // Squared response curve (progressive sensitivity)

        // Initialize current axis values
        _currentAxisX = 0.0;
        _currentAxisY = 0.0;

        // Attitude mode zeroing (will be set on first motion update)
        _referenceRoll = 0.0;
        _referenceYaw = 0.0;
        _referencePitch = 0.0;

        // Integration mode properties (for future use)
        _gyroCalibrationX = 0.0;
        _gyroCalibrationY = 0.0;
        _lastUpdateTime = nil;

        NSLog(@"[MotionController] Initialized in Attitude mode - Sensitivity: %.1f degrees for full deflection", _sensitivity);
    }
    return self;
}

- (void)startMotionUpdates {
    if (!self.motionManager.isDeviceMotionAvailable) {
        NSLog(@"[MotionController] Device motion not available");
        return;
    }

    if (self.motionManager.isDeviceMotionActive) {
        NSLog(@"[MotionController] Motion updates already active");
        return;
    }

    // Use CMAttitudeReferenceFrameXArbitraryZVertical for landscape orientation
    [self.motionManager startDeviceMotionUpdatesUsingReferenceFrame:CMAttitudeReferenceFrameXArbitraryZVertical
                                                            toQueue:[NSOperationQueue mainQueue]
                                                        withHandler:^(CMDeviceMotion *motion, NSError *error) {
        if (error) {
            NSLog(@"[MotionController] Motion update error: %@", error.localizedDescription);
            return;
        }

        if (!self.enabled) {
            return;
        }

        [self processMotionData:motion];
    }];

    NSLog(@"[MotionController] Started motion updates");
}

- (void)stopMotionUpdates {
    if (self.motionManager.isDeviceMotionActive) {
        [self.motionManager stopDeviceMotionUpdates];
        NSLog(@"[MotionController] Stopped motion updates");
    }
}

- (void)recalibrate {
    if (self.motionManager.deviceMotion) {
        if (self.mode == MotionControlModeAttitude) {
            // Attitude mode: store current attitude as the reference/zero point
            // This allows the user to hold the device at any comfortable angle
            CMAttitude *attitude = self.motionManager.deviceMotion.attitude;
            self.referenceRoll = attitude.roll;
            self.referenceYaw = attitude.yaw;
            self.referencePitch = attitude.pitch;

            NSLog(@"[MotionController] Attitude zeroed - Roll: %.2f°, Yaw: %.2f°, Pitch: %.2f°",
                  self.referenceRoll * RAD_TO_DEG, self.referenceYaw * RAD_TO_DEG, self.referencePitch * RAD_TO_DEG);
        } else {
            // Integration mode: store rotation rate bias
            CMRotationRate rotationRate = self.motionManager.deviceMotion.rotationRate;
            self.gyroCalibrationY = rotationRate.y;
            self.gyroCalibrationX = rotationRate.x;

            NSLog(@"[MotionController] Integration calibration - GyroX: %.4f rad/s, GyroY: %.4f rad/s",
                  self.gyroCalibrationX, self.gyroCalibrationY);
        }
    }
}

- (void)processMotionData:(CMDeviceMotion *)motion {
    // Store raw rotation rates for debugging and barrel roll detection
    CMRotationRate rotationRate = motion.rotationRate;
    self.rawRotX = rotationRate.x;
    self.rawRotY = rotationRate.y;
    self.rawRotZ = rotationRate.z;

    if (self.mode == MotionControlModeAttitude) {
        // ATTITUDE MODE: Direct tilt angle → stick position mapping
        // This provides natural "assist" behavior that complements analog stick input

        // Get device attitude (orientation angles)
        CMAttitude *attitude = motion.attitude;

        // Auto-zero on first update (so current holding position is neutral)
        if (self.referenceRoll == 0.0 && self.referenceYaw == 0.0 && self.referencePitch == 0.0) {
            self.referenceRoll = attitude.roll;
            self.referenceYaw = attitude.yaw;
            self.referencePitch = attitude.pitch;
            NSLog(@"[MotionController] Auto-zeroed attitude - Roll: %.2f°, Yaw: %.2f°, Pitch: %.2f°",
                  self.referenceRoll * RAD_TO_DEG, self.referenceYaw * RAD_TO_DEG, self.referencePitch * RAD_TO_DEG);
        }

        // Calculate relative attitude (current - reference)
        // This makes the user's holding angle the neutral position
        double relativeRoll = attitude.roll - self.referenceRoll;
        double relativeYaw = attitude.yaw - self.referenceYaw;
        double relativePitch = attitude.pitch - self.referencePitch;

        // Store relative pitch for barrel roll detection
        self.currentRelativePitch = relativePitch;

        // For landscape orientation:
        // - attitude.yaw (Z rotation): tilting device left/right → horizontal aiming
        // - attitude.roll (X rotation): tilting device forward/back → vertical aiming
        // - attitude.pitch (Y rotation): NOT used for stick, only for barrel roll buttons

        // Convert radians to degrees
        double rollDegrees = relativeRoll * RAD_TO_DEG;
        double yawDegrees = relativeYaw * RAD_TO_DEG;

        // Map tilt angle to axis value (SWAPPED: yaw→X, roll→Y)
        // sensitivity = degrees needed for full deflection (default 20°)
        CGFloat rawAxisX = (CGFloat)(yawDegrees / self.sensitivity);
        CGFloat rawAxisY = (CGFloat)(rollDegrees / self.sensitivity);

        // Apply deadzone (small deadzone with smooth scaling)
        CGFloat deadzoneAxisUnits = self.deadzone / self.sensitivity;
        CGFloat afterDeadzoneX = [self applyDeadzone:rawAxisX withDeadzone:deadzoneAxisUnits];
        CGFloat afterDeadzoneY = [self applyDeadzone:rawAxisY withDeadzone:deadzoneAxisUnits];

        // Apply response curve for progressive sensitivity
        // This gives fine control at small tilts, stronger response at large tilts
        CGFloat newAxisX = [self applyResponseCurve:afterDeadzoneX];
        CGFloat newAxisY = [self applyResponseCurve:afterDeadzoneY];

        // Clamp to -1.0 to 1.0 range
        newAxisX = fmax(-1.0, fmin(1.0, newAxisX));
        newAxisY = fmax(-1.0, fmin(1.0, newAxisY));

        // Apply inversion if needed
        if (self.invertRoll) {
            newAxisX = -newAxisX;
        }
        if (self.invertPitch) {
            newAxisY = -newAxisY;
        }

        // Update current axis values
        self.currentAxisX = newAxisX;
        self.currentAxisY = newAxisY;

    } else {
        // INTEGRATION MODE: Rate-based mouse-like aiming (for future feature)
        // TODO: Implement integration mode when mode toggle feature is added
        // This would use rotationRate and integrate over time like the old implementation

        self.currentAxisX = 0.0;
        self.currentAxisY = 0.0;
    }
}

- (CGFloat)applyDeadzone:(CGFloat)value withDeadzone:(CGFloat)deadzone {
    if (fabs(value) < deadzone) {
        return 0.0;
    }

    // Scale the value to maintain smooth transition after deadzone
    // Maps [deadzone, 1.0] to [0.0, 1.0]
    CGFloat sign = (value > 0) ? 1.0 : -1.0;
    CGFloat absValue = fabs(value);
    CGFloat scaledValue = (absValue - deadzone) / (1.0 - deadzone);

    return sign * scaledValue;
}

- (CGFloat)applyResponseCurve:(CGFloat)value {
    if (self.responseCurve == 1.0) {
        // Linear response (no curve)
        return value;
    }

    // Apply power curve for progressive sensitivity
    // responseCurve = 2.0: squared (gentle → aggressive)
    // responseCurve = 3.0: cubed (very gentle → very aggressive)
    CGFloat sign = (value > 0) ? 1.0 : -1.0;
    CGFloat absValue = fabs(value);
    CGFloat curved = pow(absValue, self.responseCurve);

    return sign * curved;
}

- (CGFloat)axisX {
    return self.enabled ? self.currentAxisX : 0.0;
}

- (CGFloat)axisY {
    return self.enabled ? self.currentAxisY : 0.0;
}

- (double)relativePitchDegrees {
    return self.enabled ? (self.currentRelativePitch * RAD_TO_DEG) : 0.0;
}

- (double)rawRotationRateX {
    return self.rawRotX;
}

- (double)rawRotationRateY {
    return self.rawRotY;
}

- (double)rawRotationRateZ {
    return self.rawRotZ;
}

- (void)dealloc {
    [self stopMotionUpdates];
}

@end
