//
//  StarshipViewController.mm
//  Starship iOS
//
//  Implementation of touch controls for iOS
//  Uses Objective-C++ to bridge between UIKit and C++ game engine
//

#import "StarshipViewController.h"
#import "StarshipBridge.h"
#import "MotionController.h"
#import <GameController/GameController.h>

// ============================================================================
// Touch Control Configuration
// ============================================================================

// Toggle to show/hide on-screen Z/R shoulder buttons (for barrel rolls)
// Set to NO when using gyro-based barrel rolls
static const BOOL SHOW_SHOULDER_BUTTONS = NO;

// ============================================================================
// Touch Control Button View
// Simple button overlay for on-screen controls
// ============================================================================
@interface TouchButton : UIView
@property (nonatomic, assign) int buttonIndex;
@property (nonatomic, copy) void (^onPress)(void);
@property (nonatomic, copy) void (^onRelease)(void);
@end

@implementation TouchButton

- (instancetype)initWithFrame:(CGRect)frame {
    self = [super initWithFrame:frame];
    if (self) {
        self.backgroundColor = [[UIColor whiteColor] colorWithAlphaComponent:0.7];
        self.layer.cornerRadius = frame.size.width / 2;
        self.layer.borderWidth = 3;
        self.layer.borderColor = [[UIColor whiteColor] colorWithAlphaComponent:0.9].CGColor;
        self.userInteractionEnabled = YES;
    }
    return self;
}

- (void)touchesBegan:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event {
    self.backgroundColor = [[UIColor whiteColor] colorWithAlphaComponent:0.5];
    if (self.onPress) {
        self.onPress();
    }
}

- (void)touchesEnded:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event {
    self.backgroundColor = [[UIColor whiteColor] colorWithAlphaComponent:0.2];
    if (self.onRelease) {
        self.onRelease();
    }
}

- (void)touchesCancelled:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event {
    [self touchesEnded:touches withEvent:event];
}

@end

// ============================================================================
// Virtual Joystick View
// Draggable analog stick for movement controls
// ============================================================================
@interface VirtualJoystick : UIView
@property (nonatomic, assign) int axisX;
@property (nonatomic, assign) int axisY;
@property (nonatomic, strong) UIView *knob;
@property (nonatomic, assign) CGPoint centerPoint;
@property (nonatomic, assign) CGFloat maxDistance;
@property (nonatomic, assign) CGPoint initialTouchPoint;  // Where user first touched
@property (nonatomic, assign) BOOL isTracking;            // Whether we're currently tracking a touch
@property (nonatomic, assign) CGFloat deadzoneRadius;     // Deadzone radius in pixels

// Callback for value changes (for combining with gyro input)
@property (nonatomic, copy) void (^onValueChange)(CGFloat x, CGFloat y);  // Values in -1.0 to 1.0 range
@end

@implementation VirtualJoystick

- (instancetype)initWithFrame:(CGRect)frame {
    self = [super initWithFrame:frame];
    if (self) {
        self.backgroundColor = [UIColor clearColor];

        // Create hexagonal gate shape for the joystick base
        CGFloat radius = frame.size.width / 2;
        CGPoint center = CGPointMake(radius, radius);
        UIBezierPath *hexagonPath = [UIBezierPath bezierPath];

        // Draw hexagon (6 sides, gated analog stick)
        for (int i = 0; i < 6; i++) {
            CGFloat angle = (M_PI / 3.0) * i - M_PI / 2;  // Start from top
            CGFloat x = center.x + radius * cos(angle);
            CGFloat y = center.y + radius * sin(angle);

            if (i == 0) {
                [hexagonPath moveToPoint:CGPointMake(x, y)];
            } else {
                [hexagonPath addLineToPoint:CGPointMake(x, y)];
            }
        }
        [hexagonPath closePath];

        // Create shape layer for hexagon background
        CAShapeLayer *hexagonLayer = [CAShapeLayer layer];
        hexagonLayer.path = hexagonPath.CGPath;
        hexagonLayer.fillColor = [[UIColor whiteColor] colorWithAlphaComponent:0.3].CGColor;
        hexagonLayer.strokeColor = [[UIColor whiteColor] colorWithAlphaComponent:0.8].CGColor;
        hexagonLayer.lineWidth = 3;
        [self.layer addSublayer:hexagonLayer];

        // Create draggable knob (circular)
        CGFloat knobSize = frame.size.width * 0.4;
        self.knob = [[UIView alloc] initWithFrame:CGRectMake(
            (frame.size.width - knobSize) / 2,
            (frame.size.height - knobSize) / 2,
            knobSize, knobSize
        )];
        self.knob.backgroundColor = [[UIColor whiteColor] colorWithAlphaComponent:0.5];
        self.knob.layer.cornerRadius = knobSize / 2;
        self.knob.layer.borderWidth = 2;
        self.knob.layer.borderColor = [[UIColor whiteColor] colorWithAlphaComponent:0.9].CGColor;
        [self addSubview:self.knob];

        self.centerPoint = CGPointMake(frame.size.width / 2, frame.size.height / 2);
        self.maxDistance = frame.size.width / 2 - knobSize / 2;

        // Set deadzone to 25% of max distance for adaptive centering
        self.deadzoneRadius = self.maxDistance * 0.25;
        self.isTracking = NO;
        self.initialTouchPoint = CGPointZero;

        self.userInteractionEnabled = YES;
    }
    return self;
}

- (void)touchesBegan:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event {
    UITouch *touch = [touches anyObject];
    CGPoint location = [touch locationInView:self];

    // Capture initial touch point as the new "center" for this gesture
    self.initialTouchPoint = location;
    self.isTracking = YES;

    // Don't move knob or send input until user drags past deadzone
    // This ensures the stick always "starts" from center
}

- (void)touchesMoved:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event {
    if (!self.isTracking) return;
    [self handleTouch:touches];
}

- (void)touchesEnded:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event {
    // Reset to visual center
    self.knob.center = self.centerPoint;
    self.isTracking = NO;
    self.initialTouchPoint = CGPointZero;

    // Reset input axes to zero via callback
    if (self.onValueChange) {
        self.onValueChange(0.0, 0.0);
    }
}

- (void)touchesCancelled:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event {
    [self touchesEnded:touches withEvent:event];
}

- (void)handleTouch:(NSSet<UITouch *> *)touches {
    UITouch *touch = [touches anyObject];
    CGPoint location = [touch locationInView:self];

    // Calculate offset from INITIAL TOUCH POINT (not visual center)
    CGFloat dx = location.x - self.initialTouchPoint.x;
    CGFloat dy = location.y - self.initialTouchPoint.y;
    CGFloat distance = sqrt(dx * dx + dy * dy);

    // Apply adaptive deadzone - no input until user drags past it
    if (distance < self.deadzoneRadius) {
        // Within deadzone - keep knob at center and send no input
        self.knob.center = self.centerPoint;
        if (self.onValueChange) {
            self.onValueChange(0.0, 0.0);
        }
        return;
    }

    // Scale the input so deadzone maps to 0 and maxDistance maps to 1.0
    // This ensures smooth transition after leaving deadzone
    CGFloat adjustedDistance = distance - self.deadzoneRadius;
    CGFloat adjustedMaxDistance = self.maxDistance - self.deadzoneRadius;
    CGFloat ratio = adjustedDistance / adjustedMaxDistance;

    // Clamp to max range
    if (ratio > 1.0) {
        ratio = 1.0;
        adjustedDistance = adjustedMaxDistance;
    }

    // Calculate final deltas (scaled to account for deadzone removal)
    CGFloat finalDx = (dx / distance) * adjustedDistance;
    CGFloat finalDy = (dy / distance) * adjustedDistance;

    // Update knob position (relative to visual center)
    self.knob.center = CGPointMake(self.centerPoint.x + finalDx, self.centerPoint.y + finalDy);

    // Calculate normalized axis values (-1.0 to 1.0)
    CGFloat normalizedX = finalDx / adjustedMaxDistance;
    CGFloat normalizedY = finalDy / adjustedMaxDistance;

    // Call callback with normalized values (will be combined with gyro in ViewController)
    if (self.onValueChange) {
        self.onValueChange(normalizedX, normalizedY);
    }
}

@end

// ============================================================================
// Main View Controller Implementation
// ============================================================================
@interface StarshipViewController ()
@property (nonatomic, strong) UIView *touchControlsContainer;
@property (nonatomic, assign) BOOL touchControlsVisible;
@property (nonatomic, assign) BOOL usingTouchControls;
@property (nonatomic, assign) BOOL sdlControllerInitialized;
@property (nonatomic, strong) NSTimer *motionUpdateTimer;
@property (nonatomic, strong) UILabel *gyroDebugLabel;

// Virtual stick input tracking (for combining with gyro)
@property (nonatomic, assign) CGFloat virtualStickX;  // -1.0 to 1.0
@property (nonatomic, assign) CGFloat virtualStickY;  // -1.0 to 1.0

// Auto-hold timers for Boost/Brake buttons (3.2 second hold on tap)
@property (nonatomic, strong) NSTimer *boostHoldTimer;
@property (nonatomic, strong) NSTimer *brakeHoldTimer;

// Barrel roll sequence tracking
@property (nonatomic, assign) BOOL barrelRollInProgress;
@end

@implementation StarshipViewController

- (void)viewDidLoad {
    [super viewDidLoad];

    self.view.backgroundColor = [UIColor blackColor];
    self.usingTouchControls = YES;
    self.sdlControllerInitialized = NO;

    // Initialize virtual stick values
    self.virtualStickX = 0.0;
    self.virtualStickY = 0.0;

    // Initialize barrel roll flag
    self.barrelRollInProgress = NO;

    // Setup touch controls overlay
    [self setupTouchControls];

    // Monitor for game controller connections
    [self setupGameControllerNotifications];

    // Setup motion controls for gyroscope input
    [self setupMotionControls];

    // Note: iOS_AttachController() will be called after SDL initializes
    // via delayed initialization to ensure SDL's joystick subsystem is ready
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(0.5 * NSEC_PER_SEC)), dispatch_get_main_queue(), ^{
        iOS_AttachController();
        self.sdlControllerInitialized = YES;
        NSLog(@"[iOS] SDL controller initialized and attached");
    });
}

- (void)dealloc {
    // Motion controls cleanup
    [self.motionUpdateTimer invalidate];
    [[MotionController sharedController] stopMotionUpdates];

    // Auto-hold timer cleanup
    [self.boostHoldTimer invalidate];
    [self.brakeHoldTimer invalidate];

    iOS_DetachController();
    [[NSNotificationCenter defaultCenter] removeObserver:self];
}

// ============================================================================
// Touch Controls Setup - Star Fox 64 Layout  
// Creates virtual on-screen buttons and joysticks optimized for SF64
// ============================================================================
- (void)setupTouchControls {
    // Create weak reference to self for use in blocks
    __weak typeof(self) weakSelf = self;

    self.touchControlsContainer = [[UIView alloc] initWithFrame:self.view.bounds];
    self.touchControlsContainer.autoresizingMask = UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight;
    self.touchControlsContainer.userInteractionEnabled = YES;
    self.touchControlsContainer.backgroundColor = [UIColor clearColor];  // Transparent background
    // NOTE: Don't add to self.view here - will be added to overlay window in ViewIntegration.mm

    CGFloat screenWidth = self.view.bounds.size.width;
    CGFloat screenHeight = self.view.bounds.size.height;
    CGFloat margin = 20;
    CGFloat buttonSize = 70;
    CGFloat joystickSize = 140;
    CGFloat safeAreaInset = self.view.safeAreaInsets.top > 0 ? self.view.safeAreaInsets.top : 20;

    // ========================================================================
    // LEFT SIDE: Analog Stick for Arwing Movement
    // Vertically aligned with A button center for ergonomic thumb positioning
    // ========================================================================
    VirtualJoystick *movementStick = [[VirtualJoystick alloc] initWithFrame:CGRectMake(
        margin + 35, screenHeight - margin - joystickSize - 77.5,
        joystickSize, joystickSize
    )];
    movementStick.axisX = IOS_AXIS_LEFTX;
    movementStick.axisY = IOS_AXIS_LEFTY;

    // Set callback to update ViewController's virtual stick values (for combining with gyro)
    movementStick.onValueChange = ^(CGFloat x, CGFloat y) {
        weakSelf.virtualStickX = x;
        weakSelf.virtualStickY = y;
    };

    [self.touchControlsContainer addSubview:movementStick];

    // ========================================================================
    // RIGHT SIDE: Action Buttons
    // ========================================================================

    // A Button - FIRE LASER (larger size, positioned in lower right)
    CGFloat largeButtonSize = 95;  // Made bigger
    CGFloat aButtonLeft = screenWidth - margin - largeButtonSize - 40;
    CGFloat aButtonTop = screenHeight - margin - largeButtonSize - 100;
    TouchButton *aButton = [[TouchButton alloc] initWithFrame:CGRectMake(
        aButtonLeft,
        aButtonTop,
        largeButtonSize, largeButtonSize
    )];
    aButton.onPress = ^{ iOS_SetButton(IOS_BUTTON_A, true); };
    aButton.onRelease = ^{ iOS_SetButton(IOS_BUTTON_A, false); };
    [self.touchControlsContainer addSubview:aButton];

    UILabel *aLabel = [[UILabel alloc] initWithFrame:aButton.bounds];
    aLabel.text = @"A\nFIRE";
    aLabel.numberOfLines = 2;
    aLabel.textAlignment = NSTextAlignmentCenter;
    aLabel.textColor = [UIColor whiteColor];
    aLabel.font = [UIFont boldSystemFontOfSize:22];
    [aButton addSubview:aLabel];

    // B Button - BOMB (smaller size, positioned above and to the right of A button)
    CGFloat smallButtonSize = 55;  // Made smaller
    CGFloat bButtonGap = 10;  // Gap from A button
    TouchButton *bButton = [[TouchButton alloc] initWithFrame:CGRectMake(
        screenWidth - margin - smallButtonSize,  // Align to right edge
        aButtonTop - smallButtonSize - bButtonGap,  // Above A button
        smallButtonSize, smallButtonSize
    )];
    // B button maps to X for Bomb in Star Fox 64 custom layout
    bButton.onPress = ^{ iOS_SetButton(IOS_BUTTON_X, true); };
    bButton.onRelease = ^{ iOS_SetButton(IOS_BUTTON_X, false); };
    [self.touchControlsContainer addSubview:bButton];

    UILabel *bLabel = [[UILabel alloc] initWithFrame:bButton.bounds];
    bLabel.text = @"B\nBOMB";
    bLabel.numberOfLines = 2;
    bLabel.textAlignment = NSTextAlignmentCenter;
    bLabel.textColor = [UIColor whiteColor];
    bLabel.font = [UIFont boldSystemFontOfSize:14];
    [bButton addSubview:bLabel];

    // Barrel Roll Button - positioned top-left of A button (opposite of B)
    TouchButton *barrelRollButton = [[TouchButton alloc] initWithFrame:CGRectMake(
        aButtonLeft - smallButtonSize - bButtonGap,  // Left of A button
        aButtonTop - smallButtonSize - bButtonGap,   // Above A button
        smallButtonSize, smallButtonSize
    )];
    // Simple Z button press with timestamp logging for timing analysis
    barrelRollButton.onPress = ^{
        weakSelf.barrelRollInProgress = YES;  // Disable gyro barrel roll detection
        NSLog(@"[BarrelRoll] Z PRESSED at %.3f", CACurrentMediaTime());
        iOS_SetButton(IOS_BUTTON_LEFTSHOULDER, true);
    };
    barrelRollButton.onRelease = ^{
        NSLog(@"[BarrelRoll] Z RELEASED at %.3f", CACurrentMediaTime());
        iOS_SetButton(IOS_BUTTON_LEFTSHOULDER, false);
        weakSelf.barrelRollInProgress = NO;  // Re-enable gyro barrel roll detection
    };
    [self.touchControlsContainer addSubview:barrelRollButton];

    UILabel *barrelRollLabel = [[UILabel alloc] initWithFrame:barrelRollButton.bounds];
    barrelRollLabel.text = @"ZZ\nROLL";
    barrelRollLabel.numberOfLines = 2;
    barrelRollLabel.textAlignment = NSTextAlignmentCenter;
    barrelRollLabel.textColor = [UIColor whiteColor];
    barrelRollLabel.font = [UIFont boldSystemFontOfSize:14];
    [barrelRollButton addSubview:barrelRollLabel];

    // ========================================================================
    // C-BUTTONS: N64 Star Fox 64 C-button functions
    // BOOST (C-Left) and BRAKE (C-Down): Wrap-around buttons positioned close to A button
    // Positioned to allow simultaneous brake+fire mechanic with thumb
    // CAM and MSG: Smaller semi-transparent buttons on left side
    // ========================================================================

    // Calculate A button center position (using already-defined aButtonLeft and aButtonTop)
    CGFloat aButtonCenterX = aButtonLeft + (largeButtonSize / 2);
    CGFloat aButtonCenterY = aButtonTop + (largeButtonSize / 2);

    // BOOST and BRAKE: Pill-shaped buttons wrapping around A button in rounded fashion
    CGFloat wrapButtonWidth = 60;   // Slightly wider for better pill shape
    CGFloat wrapButtonHeight = 45;  // Height for comfortable thumb reach
    CGFloat wrapGap = 3;  // Gap from A button

    // C-Left - BOOST (left side, wrapping around A button in rounded fashion)
    TouchButton *cLeft = [[TouchButton alloc] initWithFrame:CGRectMake(
        aButtonLeft - wrapButtonWidth - wrapGap,  // Positioned left of A button
        aButtonCenterY - (wrapButtonHeight / 2),  // Vertically centered with A
        wrapButtonWidth, wrapButtonHeight
    )];
    // C-Left/Boost maps to Y button in Star Fox 64 custom layout
    // Tap to hold for 3.2 seconds automatically
    cLeft.onPress = ^{
        // Cancel any existing boost timer
        [weakSelf.boostHoldTimer invalidate];

        // Press the button
        iOS_SetButton(IOS_BUTTON_Y, true);

        // Auto-release after 3.2 seconds
        weakSelf.boostHoldTimer = [NSTimer scheduledTimerWithTimeInterval:3.2
                                                                   repeats:NO
                                                                     block:^(NSTimer *timer) {
            iOS_SetButton(IOS_BUTTON_Y, false);
        }];
    };
    cLeft.onRelease = ^{
        // Ignore manual release - let timer handle it
    };
    cLeft.layer.cornerRadius = wrapButtonHeight / 2;  // Pill shape: half of height
    [self.touchControlsContainer addSubview:cLeft];

    UILabel *cLeftLabel = [[UILabel alloc] initWithFrame:cLeft.bounds];
    cLeftLabel.text = @"BOOST";
    cLeftLabel.numberOfLines = 1;
    cLeftLabel.textAlignment = NSTextAlignmentCenter;
    cLeftLabel.textColor = [UIColor whiteColor];
    cLeftLabel.font = [UIFont boldSystemFontOfSize:12];
    [cLeft addSubview:cLeftLabel];

    // C-Down - BRAKE (below A button, wrapping around in rounded fashion)
    TouchButton *cDown = [[TouchButton alloc] initWithFrame:CGRectMake(
        aButtonCenterX - (wrapButtonWidth / 2),  // Horizontally centered with A
        aButtonTop + largeButtonSize + wrapGap,  // Positioned below A button
        wrapButtonWidth, wrapButtonHeight
    )];
    // C-Down/Brake maps to B button in Star Fox 64 custom layout
    // Tap to hold for 3.2 seconds automatically
    cDown.onPress = ^{
        // Cancel any existing brake timer
        [weakSelf.brakeHoldTimer invalidate];

        // Press the button
        iOS_SetButton(IOS_BUTTON_B, true);

        // Auto-release after 3.2 seconds
        weakSelf.brakeHoldTimer = [NSTimer scheduledTimerWithTimeInterval:3.2
                                                                   repeats:NO
                                                                     block:^(NSTimer *timer) {
            iOS_SetButton(IOS_BUTTON_B, false);
        }];
    };
    cDown.onRelease = ^{
        // Ignore manual release - let timer handle it
    };
    cDown.layer.cornerRadius = wrapButtonHeight / 2;  // Pill shape: half of height
    [self.touchControlsContainer addSubview:cDown];

    UILabel *cDownLabel = [[UILabel alloc] initWithFrame:cDown.bounds];
    cDownLabel.text = @"BRAKE";
    cDownLabel.numberOfLines = 1;
    cDownLabel.textAlignment = NSTextAlignmentCenter;
    cDownLabel.textColor = [UIColor whiteColor];
    cDownLabel.font = [UIFont boldSystemFontOfSize:13];
    [cDown addSubview:cDownLabel];

    // C-Up - CAMERA ANGLE (smaller, semi-transparent, top left)
    CGFloat cButtonSmallSize = 50;
    TouchButton *cUp = [[TouchButton alloc] initWithFrame:CGRectMake(
        margin + 10, margin + safeAreaInset + 10,
        cButtonSmallSize, cButtonSmallSize
    )];
    cUp.onPress = ^{ iOS_SetButton(IOS_BUTTON_DPAD_UP, true); };
    cUp.onRelease = ^{ iOS_SetButton(IOS_BUTTON_DPAD_UP, false); };
    cUp.layer.cornerRadius = cButtonSmallSize / 2;
    cUp.alpha = 0.5;  // Semi-transparent
    [self.touchControlsContainer addSubview:cUp];

    UILabel *cUpLabel = [[UILabel alloc] initWithFrame:cUp.bounds];
    cUpLabel.text = @"CAM";
    cUpLabel.numberOfLines = 1;
    cUpLabel.textAlignment = NSTextAlignmentCenter;
    cUpLabel.textColor = [UIColor whiteColor];
    cUpLabel.font = [UIFont boldSystemFontOfSize:12];
    [cUp addSubview:cUpLabel];

    // C-Right - RESPOND TO MESSAGES (smaller, semi-transparent, top left next to CAM)
    TouchButton *cRight = [[TouchButton alloc] initWithFrame:CGRectMake(
        margin + 10 + cButtonSmallSize + 10, margin + safeAreaInset + 10,
        cButtonSmallSize, cButtonSmallSize
    )];
    cRight.onPress = ^{ iOS_SetButton(IOS_BUTTON_DPAD_RIGHT, true); };
    cRight.onRelease = ^{ iOS_SetButton(IOS_BUTTON_DPAD_RIGHT, false); };
    cRight.layer.cornerRadius = cButtonSmallSize / 2;
    cRight.alpha = 0.5;  // Semi-transparent
    [self.touchControlsContainer addSubview:cRight];

    UILabel *cRightLabel = [[UILabel alloc] initWithFrame:cRight.bounds];
    cRightLabel.text = @"MSG";
    cRightLabel.numberOfLines = 1;
    cRightLabel.textAlignment = NSTextAlignmentCenter;
    cRightLabel.textColor = [UIColor whiteColor];
    cRightLabel.font = [UIFont boldSystemFontOfSize:12];
    [cRight addSubview:cRightLabel];

    // ========================================================================
    // BOTTOM CORNER BUTTONS: Z and R for tilt/barrel roll
    // Positioned in bottom left and bottom right corners
    // Only shown if SHOW_SHOULDER_BUTTONS is YES (disabled when using gyro)
    // ========================================================================
    if (SHOW_SHOULDER_BUTTONS) {
        CGFloat cornerButtonWidth = 80;
        CGFloat cornerButtonHeight = 100;

        // Z Button - TILT LEFT (bottom left corner)
        TouchButton *zButton = [[TouchButton alloc] initWithFrame:CGRectMake(
            0, screenHeight - cornerButtonHeight, cornerButtonWidth, cornerButtonHeight
        )];
        zButton.layer.cornerRadius = 12;
        zButton.onPress = ^{ iOS_SetButton(IOS_BUTTON_LEFTSHOULDER, true); };
        zButton.onRelease = ^{ iOS_SetButton(IOS_BUTTON_LEFTSHOULDER, false); };
        [self.touchControlsContainer addSubview:zButton];

        UILabel *zLabel = [[UILabel alloc] initWithFrame:zButton.bounds];
        zLabel.text = @"Z\nTILT\n←";
        zLabel.numberOfLines = 3;
        zLabel.textAlignment = NSTextAlignmentCenter;
        zLabel.textColor = [UIColor whiteColor];
        zLabel.font = [UIFont boldSystemFontOfSize:18];
        [zButton addSubview:zLabel];

        // R Button - TILT RIGHT (bottom right corner)
        TouchButton *rButton = [[TouchButton alloc] initWithFrame:CGRectMake(
            screenWidth - cornerButtonWidth, screenHeight - cornerButtonHeight,
            cornerButtonWidth, cornerButtonHeight
        )];
        rButton.layer.cornerRadius = 12;
        rButton.onPress = ^{ iOS_SetButton(IOS_BUTTON_RIGHTSHOULDER, true); };
        rButton.onRelease = ^{ iOS_SetButton(IOS_BUTTON_RIGHTSHOULDER, false); };
        [self.touchControlsContainer addSubview:rButton];

        UILabel *rLabel = [[UILabel alloc] initWithFrame:rButton.bounds];
        rLabel.text = @"R\nTILT\n→";
        rLabel.numberOfLines = 3;
        rLabel.textAlignment = NSTextAlignmentCenter;
        rLabel.textColor = [UIColor whiteColor];
        rLabel.font = [UIFont boldSystemFontOfSize:18];
        [rButton addSubview:rLabel];
    }

    // ========================================================================
    // START BUTTON: Top left, semi-transparent (next to CAM and MSG)
    // ========================================================================
    TouchButton *startButton = [[TouchButton alloc] initWithFrame:CGRectMake(
        margin + 10 + (cButtonSmallSize + 10) * 2, margin + safeAreaInset + 10,
        buttonSize * 0.8, buttonSize * 0.6
    )];
    startButton.layer.cornerRadius = 8;
    startButton.alpha = 0.5;  // Semi-transparent
    startButton.onPress = ^{ iOS_SetButton(IOS_BUTTON_START, true); };
    startButton.onRelease = ^{ iOS_SetButton(IOS_BUTTON_START, false); };
    [self.touchControlsContainer addSubview:startButton];

    UILabel *startLabel = [[UILabel alloc] initWithFrame:startButton.bounds];
    startLabel.text = @"START";
    startLabel.textAlignment = NSTextAlignmentCenter;
    startLabel.textColor = [UIColor whiteColor];
    startLabel.font = [UIFont boldSystemFontOfSize:14];
    [startButton addSubview:startLabel];

    // Explicitly show touch controls by default
    self.touchControlsContainer.hidden = NO;
    self.touchControlsVisible = YES;
    NSLog(@"[iOS] Star Fox 64 touch controls setup complete. Container hidden: %d, frame: %@",
          self.touchControlsContainer.hidden,
          NSStringFromCGRect(self.touchControlsContainer.frame));
}

// ============================================================================
// Motion Controls Setup - Gyroscope Aiming
// Uses device motion to control right analog stick for aiming in Star Fox 64
// ============================================================================
- (void)setupMotionControls {
    // Start motion updates from the gyroscope
    [[MotionController sharedController] startMotionUpdates];

    // Calibrate to current device orientation
    [[MotionController sharedController] recalibrate];

    // Create debug label to display gyro values in real-time
    self.gyroDebugLabel = [[UILabel alloc] initWithFrame:CGRectMake(10, 50, 300, 100)];
    self.gyroDebugLabel.numberOfLines = 0;
    self.gyroDebugLabel.font = [UIFont monospacedSystemFontOfSize:10 weight:UIFontWeightRegular];
    self.gyroDebugLabel.textColor = [UIColor greenColor];
    self.gyroDebugLabel.backgroundColor = [[UIColor blackColor] colorWithAlphaComponent:0.8];
    self.gyroDebugLabel.layer.cornerRadius = 5;
    self.gyroDebugLabel.clipsToBounds = YES;
    self.gyroDebugLabel.textAlignment = NSTextAlignmentLeft;
    self.gyroDebugLabel.userInteractionEnabled = NO;  // Don't block touch events

    // Add to touch controls container so it appears on the overlay window
    [self.touchControlsContainer addSubview:self.gyroDebugLabel];

    // Bring debug label to front to ensure it's visible
    [self.touchControlsContainer bringSubviewToFront:self.gyroDebugLabel];

    NSLog(@"[MotionController] Debug label created at top-left and added to overlay");

    // Create timer to update right analog stick from gyro data at 60Hz
    self.motionUpdateTimer = [NSTimer scheduledTimerWithTimeInterval:1.0/60.0
                                                              repeats:YES
                                                                block:^(NSTimer *timer) {
        // Get current gyro axis values (-1.0 to 1.0)
        CGFloat gyroX = [[MotionController sharedController] axisX];
        CGFloat gyroY = [[MotionController sharedController] axisY];

        // Get raw rotation rate values (rad/s)
        double rawX = [[MotionController sharedController] rawRotationRateX];
        double rawY = [[MotionController sharedController] rawRotationRateY];
        double rawZ = [[MotionController sharedController] rawRotationRateZ];

        // COMBINE gyro + virtual stick inputs (additive)
        // This allows gyro to assist/complement the virtual stick aiming
        CGFloat combinedX = self.virtualStickX + gyroX;
        CGFloat combinedY = self.virtualStickY + gyroY;

        // Clamp combined values to -1.0 to 1.0 range
        combinedX = fmax(-1.0, fmin(1.0, combinedX));
        combinedY = fmax(-1.0, fmin(1.0, combinedY));

        // Convert to SDL axis range (-32768 to 32767)
        // Map to LEFT analog stick (SF64 only has one stick)
        short sdlAxisX = (short)(combinedX * 32767);
        short sdlAxisY = (short)(combinedY * 32767);

        iOS_SetAxis(IOS_AXIS_LEFTX, sdlAxisX);
        iOS_SetAxis(IOS_AXIS_LEFTY, sdlAxisY);

        // Barrel roll detection using pitch attitude angle
        // Get relative pitch angle in degrees (relative to zeroed position)
        // Only process gyro barrel roll if manual barrel roll button isn't being used
        if (!self.barrelRollInProgress) {
            double relativePitch = [[MotionController sharedController] relativePitchDegrees];

            // Threshold: 8 degrees of pitch for barrel roll input (more sensitive)
            double rollThreshold = 8.0;
            if (relativePitch < -rollThreshold) {
                // Pitched forward -> Z button (roll left)
                iOS_SetButton(IOS_BUTTON_LEFTSHOULDER, true);
                iOS_SetButton(IOS_BUTTON_RIGHTSHOULDER, false);
            } else if (relativePitch > rollThreshold) {
                // Pitched backward -> R button (roll right)
                iOS_SetButton(IOS_BUTTON_RIGHTSHOULDER, true);
                iOS_SetButton(IOS_BUTTON_LEFTSHOULDER, false);
            } else {
                // No roll detected
                iOS_SetButton(IOS_BUTTON_LEFTSHOULDER, false);
                iOS_SetButton(IOS_BUTTON_RIGHTSHOULDER, false);
            }
        }

        // Check if we're hitting clamp limits
        BOOL clampedX = (fabs(gyroX) >= 0.99);
        BOOL clampedY = (fabs(gyroY) >= 0.99);

        // Determine roll button status for debug (using pitch attitude)
        NSString *rollStatus = @"---";
        if (relativePitch < -rollThreshold) {
            rollStatus = @"Z (LEFT)";
        } else if (relativePitch > rollThreshold) {
            rollStatus = @"R (RIGHT)";
        }

        // Update debug label with current values
        dispatch_async(dispatch_get_main_queue(), ^{
            self.gyroDebugLabel.text = [NSString stringWithFormat:
                @"  GYRO DEBUG (Attitude Assist Mode)  \n"
                @"Raw (rad/s) X:%+.3f Y:%+.3f Z:%+.3f\n"
                @"Gyro        X:%+.3f Y:%+.3f%@\n"
                @"VStick      X:%+.3f Y:%+.3f\n"
                @"Combined    X:%+.3f Y:%+.3f\n"
                @"SDL Axis    X:%6d Y:%6d\n"
                @"Pitch Angle: %+.1f° | Roll Button: %@\n"
                @"Sensitivity: %.1f°  Enabled: %@",
                rawX, rawY, rawZ,
                gyroX, gyroY, (clampedX || clampedY) ? @" CLAMPED!" : @"",
                self.virtualStickX, self.virtualStickY,
                combinedX, combinedY,
                sdlAxisX, sdlAxisY,
                relativePitch, rollStatus,
                [[MotionController sharedController] sensitivity],
                [[MotionController sharedController] enabled] ? @"YES" : @"NO"];
        });
    }];

    NSLog(@"[MotionController] Gyro assist mode initialized - complements virtual stick input");
}

// ============================================================================
// Game Controller Support
// Automatically hide touch controls when physical controller connected
// ============================================================================
- (void)setupGameControllerNotifications {
    [[NSNotificationCenter defaultCenter] addObserver:self
                                             selector:@selector(controllerDidConnect:)
                                                 name:GCControllerDidConnectNotification
                                               object:nil];

    [[NSNotificationCenter defaultCenter] addObserver:self
                                             selector:@selector(controllerDidDisconnect:)
                                                 name:GCControllerDidDisconnectNotification
                                               object:nil];
}

- (void)controllerDidConnect:(NSNotification *)notification {
    NSLog(@"[iOS] Game controller connected");
    [self setTouchControlsVisible:NO];
}

- (void)controllerDidDisconnect:(NSNotification *)notification {
    NSLog(@"[iOS] Game controller disconnected");
    // Only show touch controls if no other controllers connected
    if ([GCController controllers].count == 0) {
        [self setTouchControlsVisible:YES];
    }
}

// ============================================================================
// Public Interface
// ============================================================================
- (void)setTouchControlsVisible:(BOOL)visible {
    _touchControlsVisible = visible;

    // Show/hide the overlay window
    if (self.overlayWindow) {
        self.overlayWindow.hidden = !visible;
    } else {
        // Fallback to container visibility if overlay window not set up yet
        self.touchControlsContainer.hidden = !visible;
    }

    if (visible) {
        // Only attach controller if SDL has been initialized
        if (self.sdlControllerInitialized) {
            iOS_AttachController();
            self.usingTouchControls = YES;
        } else {
            NSLog(@"[iOS] Deferring controller attachment until SDL initializes");
            self.usingTouchControls = YES;
        }
    } else {
        // Only detach if SDL was initialized and controller was attached
        if (self.sdlControllerInitialized) {
            iOS_DetachController();
        }
        self.usingTouchControls = NO;
    }
}

- (BOOL)isUsingTouchControls {
    return self.usingTouchControls;
}

// ============================================================================
// Status Bar and Orientation
// ============================================================================
- (BOOL)prefersStatusBarHidden {
    return YES;
}

- (UIInterfaceOrientationMask)supportedInterfaceOrientations {
    return UIInterfaceOrientationMaskLandscape;
}

- (BOOL)shouldAutorotate {
    return YES;
}

@end
