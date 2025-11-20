#import <Foundation/Foundation.h>
#import <AVFoundation/AVFoundation.h>

// iOS audio session configuration helper
// This MUST be called before SDL initializes audio
extern "C" bool ConfigureIOSAudioSession() {
    @autoreleasepool {
        AVAudioSession *audioSession = [AVAudioSession sharedInstance];
        NSError *error = nil;

        // Set audio session category for playback
        // AVAudioSessionCategoryPlayback allows audio to play even when silent switch is on
        BOOL success = [audioSession setCategory:AVAudioSessionCategoryPlayback
                                             mode:AVAudioSessionModeDefault
                                          options:AVAudioSessionCategoryOptionMixWithOthers
                                            error:&error];

        if (!success || error) {
            NSLog(@"[iOS Audio] Failed to set audio session category: %@",
                  error ? [error localizedDescription] : @"unknown error");
            return false;
        }

        // Activate the audio session
        success = [audioSession setActive:YES error:&error];
        if (!success || error) {
            NSLog(@"[iOS Audio] Failed to activate audio session: %@",
                  error ? [error localizedDescription] : @"unknown error");
            return false;
        }

        NSLog(@"[iOS Audio] Audio session configured successfully");
        NSLog(@"[iOS Audio] Sample rate: %f Hz", [audioSession sampleRate]);
        NSLog(@"[iOS Audio] Output channels: %ld", (long)[audioSession outputNumberOfChannels]);
        NSLog(@"[iOS Audio] IO buffer duration: %f seconds", [audioSession IOBufferDuration]);

        return true;
    }
}

// Note: Audio session is now configured in osContInit() right before SDL_InitSubSystem(SDL_INIT_AUDIO)
// This ensures proper timing - after SDL is initialized but before audio subsystem init
