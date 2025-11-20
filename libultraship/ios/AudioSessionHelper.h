#ifndef AUDIO_SESSION_HELPER_H
#define AUDIO_SESSION_HELPER_H

#ifdef __cplusplus
extern "C" {
#endif

// Configure iOS audio session for playback
// Must be called before SDL_InitSubSystem(SDL_INIT_AUDIO)
// Returns true on success, false on failure
bool ConfigureIOSAudioSession();

#ifdef __cplusplus
}
#endif

#endif // AUDIO_SESSION_HELPER_H
