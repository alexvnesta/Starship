//
//  StarshipViewController.mm
//  Starship iOS
//
//  Implementation of touch controls for iOS
//  Uses Objective-C++ to bridge between UIKit and C++ game engine
//

#import "StarshipViewController.h"
#import "StarshipBridge.h"
#import <GameController/GameController.h>

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
        self.backgroundColor = [[UIColor whiteColor] colorWithAlphaComponent:0.2];
        self.layer.cornerRadius = frame.size.width / 2;
        self.layer.borderWidth = 2;
        self.layer.borderColor = [[UIColor whiteColor] colorWithAlphaComponent:0.5].CGColor;
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
@end

@implementation VirtualJoystick

- (instancetype)initWithFrame:(CGRect)frame {
    self = [super initWithFrame:frame];
    if (self) {
        self.backgroundColor = [[UIColor whiteColor] colorWithAlphaComponent:0.1];
        self.layer.cornerRadius = frame.size.width / 2;
        self.layer.borderWidth = 2;
        self.layer.borderColor = [[UIColor whiteColor] colorWithAlphaComponent:0.3].CGColor;

        // Create draggable knob
        CGFloat knobSize = frame.size.width * 0.4;
        self.knob = [[UIView alloc] initWithFrame:CGRectMake(
            (frame.size.width - knobSize) / 2,
            (frame.size.height - knobSize) / 2,
            knobSize, knobSize
        )];
        self.knob.backgroundColor = [[UIColor whiteColor] colorWithAlphaComponent:0.5];
        self.knob.layer.cornerRadius = knobSize / 2;
        [self addSubview:self.knob];

        self.centerPoint = CGPointMake(frame.size.width / 2, frame.size.height / 2);
        self.maxDistance = frame.size.width / 2 - knobSize / 2;

        self.userInteractionEnabled = YES;
    }
    return self;
}

- (void)touchesBegan:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event {
    [self handleTouch:touches];
}

- (void)touchesMoved:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event {
    [self handleTouch:touches];
}

- (void)touchesEnded:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event {
    // Reset to center
    self.knob.center = self.centerPoint;
    iOS_SetAxis(self.axisX, 0);
    iOS_SetAxis(self.axisY, 0);
}

- (void)touchesCancelled:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event {
    [self touchesEnded:touches withEvent:event];
}

- (void)handleTouch:(NSSet<UITouch *> *)touches {
    UITouch *touch = [touches anyObject];
    CGPoint location = [touch locationInView:self];

    // Calculate offset from center
    CGFloat dx = location.x - self.centerPoint.x;
    CGFloat dy = location.y - self.centerPoint.y;
    CGFloat distance = sqrt(dx * dx + dy * dy);

    // Clamp to max distance
    if (distance > self.maxDistance) {
        CGFloat ratio = self.maxDistance / distance;
        dx *= ratio;
        dy *= ratio;
        distance = self.maxDistance;
    }

    // Update knob position
    self.knob.center = CGPointMake(self.centerPoint.x + dx, self.centerPoint.y + dy);

    // Convert to axis values (-32768 to 32767)
    short axisXValue = (short)((dx / self.maxDistance) * 32767);
    short axisYValue = (short)((dy / self.maxDistance) * 32767);

    iOS_SetAxis(self.axisX, axisXValue);
    iOS_SetAxis(self.axisY, axisYValue);
}

@end

// ============================================================================
// Main View Controller Implementation
// ============================================================================
@interface StarshipViewController ()
@property (nonatomic, strong) UIView *touchControlsContainer;
@property (nonatomic, assign) BOOL touchControlsVisible;
@property (nonatomic, assign) BOOL usingTouchControls;
@end

@implementation StarshipViewController

- (void)viewDidLoad {
    [super viewDidLoad];

    self.view.backgroundColor = [UIColor blackColor];
    self.usingTouchControls = YES;

    // Setup touch controls overlay
    [self setupTouchControls];

    // Monitor for game controller connections
    [self setupGameControllerNotifications];

    // Initialize virtual controller
    iOS_AttachController();
}

- (void)dealloc {
    iOS_DetachController();
    [[NSNotificationCenter defaultCenter] removeObserver:self];
}

// ============================================================================
// Touch Controls Setup
// ============================================================================
- (void)setupTouchControls {
    self.touchControlsContainer = [[UIView alloc] initWithFrame:self.view.bounds];
    self.touchControlsContainer.autoresizingMask = UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight;
    self.touchControlsContainer.userInteractionEnabled = YES;
    [self.view addSubview:self.touchControlsContainer];

    CGFloat screenWidth = self.view.bounds.size.width;
    CGFloat screenHeight = self.view.bounds.size.height;
    CGFloat margin = 40;
    CGFloat buttonSize = 60;
    CGFloat joystickSize = 140;

    // Left virtual joystick (movement)
    VirtualJoystick *leftStick = [[VirtualJoystick alloc] initWithFrame:CGRectMake(
        margin, screenHeight - margin - joystickSize,
        joystickSize, joystickSize
    )];
    leftStick.axisX = IOS_AXIS_LEFTX;
    leftStick.axisY = IOS_AXIS_LEFTY;
    [self.touchControlsContainer addSubview:leftStick];

    // Right virtual joystick (camera)
    VirtualJoystick *rightStick = [[VirtualJoystick alloc] initWithFrame:CGRectMake(
        screenWidth - margin - joystickSize, screenHeight - margin - joystickSize,
        joystickSize, joystickSize
    )];
    rightStick.axisX = IOS_AXIS_RIGHTX;
    rightStick.axisY = IOS_AXIS_RIGHTY;
    [self.touchControlsContainer addSubview:rightStick];

    // A Button (right side, primary action)
    TouchButton *aButton = [[TouchButton alloc] initWithFrame:CGRectMake(
        screenWidth - margin - buttonSize - 100, screenHeight - margin - buttonSize - 100,
        buttonSize, buttonSize
    )];
    aButton.onPress = ^{ iOS_SetButton(IOS_BUTTON_A, true); };
    aButton.onRelease = ^{ iOS_SetButton(IOS_BUTTON_A, false); };
    [self.touchControlsContainer addSubview:aButton];

    UILabel *aLabel = [[UILabel alloc] initWithFrame:aButton.bounds];
    aLabel.text = @"A";
    aLabel.textAlignment = NSTextAlignmentCenter;
    aLabel.textColor = [UIColor whiteColor];
    aLabel.font = [UIFont boldSystemFontOfSize:24];
    [aButton addSubview:aLabel];

    // B Button (right side, secondary action)
    TouchButton *bButton = [[TouchButton alloc] initWithFrame:CGRectMake(
        screenWidth - margin - buttonSize, screenHeight - margin - buttonSize - 50,
        buttonSize, buttonSize
    )];
    bButton.onPress = ^{ iOS_SetButton(IOS_BUTTON_B, true); };
    bButton.onRelease = ^{ iOS_SetButton(IOS_BUTTON_B, false); };
    [self.touchControlsContainer addSubview:bButton];

    UILabel *bLabel = [[UILabel alloc] initWithFrame:bButton.bounds];
    bLabel.text = @"B";
    bLabel.textAlignment = NSTextAlignmentCenter;
    bLabel.textColor = [UIColor whiteColor];
    bLabel.font = [UIFont boldSystemFontOfSize:24];
    [bButton addSubview:bLabel];

    // X Button
    TouchButton *xButton = [[TouchButton alloc] initWithFrame:CGRectMake(
        screenWidth - margin - buttonSize - 50, screenHeight - margin - buttonSize - 150,
        buttonSize, buttonSize
    )];
    xButton.onPress = ^{ iOS_SetButton(IOS_BUTTON_X, true); };
    xButton.onRelease = ^{ iOS_SetButton(IOS_BUTTON_X, false); };
    [self.touchControlsContainer addSubview:xButton];

    UILabel *xLabel = [[UILabel alloc] initWithFrame:xButton.bounds];
    xLabel.text = @"X";
    xLabel.textAlignment = NSTextAlignmentCenter;
    xLabel.textColor = [UIColor whiteColor];
    xLabel.font = [UIFont boldSystemFontOfSize:24];
    [xButton addSubview:xLabel];

    // Y Button
    TouchButton *yButton = [[TouchButton alloc] initWithFrame:CGRectMake(
        screenWidth - margin - buttonSize - 150, screenHeight - margin - buttonSize - 100,
        buttonSize, buttonSize
    )];
    yButton.onPress = ^{ iOS_SetButton(IOS_BUTTON_Y, true); };
    yButton.onRelease = ^{ iOS_SetButton(IOS_BUTTON_Y, false); };
    [self.touchControlsContainer addSubview:yButton];

    UILabel *yLabel = [[UILabel alloc] initWithFrame:yButton.bounds];
    yLabel.text = @"Y";
    yLabel.textAlignment = NSTextAlignmentCenter;
    yLabel.textColor = [UIColor whiteColor];
    yLabel.font = [UIFont boldSystemFontOfSize:24];
    [yButton addSubview:yLabel];

    // Start Button (top center)
    TouchButton *startButton = [[TouchButton alloc] initWithFrame:CGRectMake(
        screenWidth / 2 + 30, margin,
        buttonSize * 0.8, buttonSize * 0.8
    )];
    startButton.onPress = ^{ iOS_SetButton(IOS_BUTTON_START, true); };
    startButton.onRelease = ^{ iOS_SetButton(IOS_BUTTON_START, false); };
    [self.touchControlsContainer addSubview:startButton];

    UILabel *startLabel = [[UILabel alloc] initWithFrame:startButton.bounds];
    startLabel.text = @"+";
    startLabel.textAlignment = NSTextAlignmentCenter;
    startLabel.textColor = [UIColor whiteColor];
    startLabel.font = [UIFont boldSystemFontOfSize:28];
    [startButton addSubview:startLabel];

    // Select/Back Button (top center left)
    TouchButton *selectButton = [[TouchButton alloc] initWithFrame:CGRectMake(
        screenWidth / 2 - 30 - buttonSize * 0.8, margin,
        buttonSize * 0.8, buttonSize * 0.8
    )];
    selectButton.onPress = ^{ iOS_SetButton(IOS_BUTTON_BACK, true); };
    selectButton.onRelease = ^{ iOS_SetButton(IOS_BUTTON_BACK, false); };
    [self.touchControlsContainer addSubview:selectButton];

    UILabel *selectLabel = [[UILabel alloc] initWithFrame:selectButton.bounds];
    selectLabel.text = @"−";
    selectLabel.textAlignment = NSTextAlignmentCenter;
    selectLabel.textColor = [UIColor whiteColor];
    selectLabel.font = [UIFont boldSystemFontOfSize:28];
    [selectButton addSubview:selectLabel];

    // D-Pad (top left)
    CGFloat dpadSize = 50;
    CGFloat dpadSpacing = 55;
    CGFloat dpadX = margin + joystickSize + 40;
    CGFloat dpadY = screenHeight - margin - joystickSize + 20;

    // D-Pad Up
    TouchButton *dpadUp = [[TouchButton alloc] initWithFrame:CGRectMake(
        dpadX, dpadY - dpadSpacing, dpadSize, dpadSize
    )];
    dpadUp.onPress = ^{ iOS_SetButton(IOS_BUTTON_DPAD_UP, true); };
    dpadUp.onRelease = ^{ iOS_SetButton(IOS_BUTTON_DPAD_UP, false); };
    [self.touchControlsContainer addSubview:dpadUp];

    // D-Pad Down
    TouchButton *dpadDown = [[TouchButton alloc] initWithFrame:CGRectMake(
        dpadX, dpadY + dpadSpacing, dpadSize, dpadSize
    )];
    dpadDown.onPress = ^{ iOS_SetButton(IOS_BUTTON_DPAD_DOWN, true); };
    dpadDown.onRelease = ^{ iOS_SetButton(IOS_BUTTON_DPAD_DOWN, false); };
    [self.touchControlsContainer addSubview:dpadDown];

    // D-Pad Left
    TouchButton *dpadLeft = [[TouchButton alloc] initWithFrame:CGRectMake(
        dpadX - dpadSpacing, dpadY, dpadSize, dpadSize
    )];
    dpadLeft.onPress = ^{ iOS_SetButton(IOS_BUTTON_DPAD_LEFT, true); };
    dpadLeft.onRelease = ^{ iOS_SetButton(IOS_BUTTON_DPAD_LEFT, false); };
    [self.touchControlsContainer addSubview:dpadLeft];

    // D-Pad Right
    TouchButton *dpadRight = [[TouchButton alloc] initWithFrame:CGRectMake(
        dpadX + dpadSpacing, dpadY, dpadSize, dpadSize
    )];
    dpadRight.onPress = ^{ iOS_SetButton(IOS_BUTTON_DPAD_RIGHT, true); };
    dpadRight.onRelease = ^{ iOS_SetButton(IOS_BUTTON_DPAD_RIGHT, false); };
    [self.touchControlsContainer addSubview:dpadRight];

    // Shoulder Buttons (top corners)
    CGFloat shoulderWidth = 100;
    CGFloat shoulderHeight = 40;

    // L Button
    TouchButton *lButton = [[TouchButton alloc] initWithFrame:CGRectMake(
        margin, margin, shoulderWidth, shoulderHeight
    )];
    lButton.layer.cornerRadius = 8;
    lButton.onPress = ^{ iOS_SetButton(IOS_BUTTON_LEFTSHOULDER, true); };
    lButton.onRelease = ^{ iOS_SetButton(IOS_BUTTON_LEFTSHOULDER, false); };
    [self.touchControlsContainer addSubview:lButton];

    UILabel *lLabel = [[UILabel alloc] initWithFrame:lButton.bounds];
    lLabel.text = @"L";
    lLabel.textAlignment = NSTextAlignmentCenter;
    lLabel.textColor = [UIColor whiteColor];
    lLabel.font = [UIFont boldSystemFontOfSize:20];
    [lButton addSubview:lLabel];

    // R Button
    TouchButton *rButton = [[TouchButton alloc] initWithFrame:CGRectMake(
        screenWidth - margin - shoulderWidth, margin, shoulderWidth, shoulderHeight
    )];
    rButton.layer.cornerRadius = 8;
    rButton.onPress = ^{ iOS_SetButton(IOS_BUTTON_RIGHTSHOULDER, true); };
    rButton.onRelease = ^{ iOS_SetButton(IOS_BUTTON_RIGHTSHOULDER, false); };
    [self.touchControlsContainer addSubview:rButton];

    UILabel *rLabel = [[UILabel alloc] initWithFrame:rButton.bounds];
    rLabel.text = @"R";
    rLabel.textAlignment = NSTextAlignmentCenter;
    rLabel.textColor = [UIColor whiteColor];
    rLabel.font = [UIFont boldSystemFontOfSize:20];
    [rButton addSubview:rLabel];

    self.touchControlsVisible = YES;
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
    self.touchControlsVisible = visible;
    self.touchControlsContainer.hidden = !visible;

    if (visible) {
        iOS_AttachController();
        self.usingTouchControls = YES;
    } else {
        iOS_DetachController();
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
