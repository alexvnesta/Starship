//
//  IOSFilePicker.mm
//  Starship iOS
//
//  Implementation of native iOS file picker using UIDocumentPickerViewController
//  Allows users to select ROM files from their device
//

#import <UIKit/UIKit.h>
#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>
#include "StarshipBridge.h"
#include <string>

// ============================================================================
// Document Picker Delegate
// Handles the result of file selection
// ============================================================================
@interface IOSDocumentPickerDelegate : NSObject <UIDocumentPickerDelegate>
@property (nonatomic, copy) NSString *selectedPath;
@property (nonatomic, assign) BOOL didComplete;
@property (nonatomic, strong) NSCondition *condition;
@end

@implementation IOSDocumentPickerDelegate

- (instancetype)init {
    self = [super init];
    if (self) {
        self.condition = [[NSCondition alloc] init];
        self.didComplete = NO;
        self.selectedPath = nil;
    }
    return self;
}

- (void)documentPicker:(UIDocumentPickerViewController *)controller didPickDocumentsAtURLs:(NSArray<NSURL *> *)urls {
    [self.condition lock];

    if (urls.count > 0) {
        NSURL *url = urls[0];

        // Start accessing the security-scoped resource
        if ([url startAccessingSecurityScopedResource]) {
            // Get the file path
            self.selectedPath = [url path];
            NSLog(@"[iOS File Picker] Selected file: %@", self.selectedPath);
        } else {
            NSLog(@"[iOS File Picker] Failed to access security-scoped resource");
        }
    }

    self.didComplete = YES;
    [self.condition signal];
    [self.condition unlock];
}

- (void)documentPickerWasCancelled:(UIDocumentPickerViewController *)controller {
    NSLog(@"[iOS File Picker] User cancelled file selection");
    [self.condition lock];
    self.didComplete = YES;
    self.selectedPath = nil;
    [self.condition signal];
    [self.condition unlock];
}

@end

// ============================================================================
// C++ Bridge Function
// ============================================================================

extern "C" {

/**
 * Show iOS file picker and return selected file path
 * This function blocks until user selects a file or cancels
 * @param outPath Buffer to store selected path (caller must provide buffer of at least 1024 bytes)
 * @param pathSize Size of the output buffer
 * @return true if file was selected, false if cancelled
 */
bool iOS_ShowFilePicker(char* outPath, size_t pathSize) {
    if (!outPath || pathSize < 1024) {
        NSLog(@"[iOS File Picker] Invalid parameters");
        return false;
    }

    NSLog(@"[iOS File Picker] Starting file picker...");
    NSLog(@"[iOS File Picker] Main thread check: %@", [NSThread isMainThread] ? @"YES" : @"NO");

    __block IOSDocumentPickerDelegate *delegate = [[IOSDocumentPickerDelegate alloc] init];
    __block BOOL presented = NO;

    // Create a block to present the picker
    void (^presentPicker)(void) = ^{
        @try {
            NSLog(@"[iOS File Picker] Creating document picker...");

            // Create document picker for all file types
            UIDocumentPickerViewController *picker = nil;

            if (@available(iOS 14.0, *)) {
                picker = [[UIDocumentPickerViewController alloc]
                    initForOpeningContentTypes:@[UTTypeItem]
                    asCopy:YES];
            } else {
                picker = [[UIDocumentPickerViewController alloc]
                    initWithDocumentTypes:@[@"public.item"]
                    inMode:UIDocumentPickerModeImport];
            }

            picker.delegate = delegate;
            picker.allowsMultipleSelection = NO;

            NSLog(@"[iOS File Picker] Getting root view controller...");

            // Get the root view controller
            UIViewController *rootVC = nil;

            // Try different methods to get the root view controller
            if (@available(iOS 15.0, *)) {
                UIWindowScene *scene = (UIWindowScene *)[[[UIApplication sharedApplication] connectedScenes] anyObject];
                if (scene) {
                    rootVC = scene.windows.firstObject.rootViewController;
                    NSLog(@"[iOS File Picker] Got root VC from scene: %@", rootVC);
                }
            }

            if (rootVC == nil) {
                rootVC = [UIApplication sharedApplication].keyWindow.rootViewController;
                NSLog(@"[iOS File Picker] Got root VC from keyWindow: %@", rootVC);
            }

            if (rootVC == nil) {
                NSLog(@"[iOS File Picker] ERROR: Could not find root view controller");
                return;
            }

            // Navigate to the topmost presented view controller
            while (rootVC.presentedViewController) {
                rootVC = rootVC.presentedViewController;
            }

            NSLog(@"[iOS File Picker] Presenting picker from: %@", rootVC);

            // Present the picker
            [rootVC presentViewController:picker animated:YES completion:^{
                NSLog(@"[iOS File Picker] Picker presented successfully");
            }];

            presented = YES;

        } @catch (NSException *exception) {
            NSLog(@"[iOS File Picker] Exception while presenting: %@", exception);
        }
    };

    // Check if we're already on the main thread
    if ([NSThread isMainThread]) {
        NSLog(@"[iOS File Picker] Already on main thread, presenting directly");
        presentPicker();
    } else {
        NSLog(@"[iOS File Picker] Not on main thread, dispatching to main thread");
        dispatch_sync(dispatch_get_main_queue(), presentPicker);
    }

    if (!presented) {
        NSLog(@"[iOS File Picker] Failed to present picker");
        return false;
    }

    // Wait for the user to select or cancel (with 5 minute timeout)
    NSLog(@"[iOS File Picker] Waiting for user selection...");
    [delegate.condition lock];

    if (!delegate.didComplete) {
        NSDate *timeoutDate = [NSDate dateWithTimeIntervalSinceNow:300.0];
        BOOL signaled = [delegate.condition waitUntilDate:timeoutDate];

        if (!signaled) {
            NSLog(@"[iOS File Picker] Timeout waiting for selection");
            [delegate.condition unlock];
            return false;
        }
    }

    BOOL result = (delegate.selectedPath != nil);

    if (result) {
        strncpy(outPath, [delegate.selectedPath UTF8String], pathSize - 1);
        outPath[pathSize - 1] = '\0';
        NSLog(@"[iOS File Picker] Returning path: %s", outPath);
    } else {
        NSLog(@"[iOS File Picker] No file selected");
    }

    [delegate.condition unlock];

    return result;
}

} // extern "C"
