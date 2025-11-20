#include "SDLAudioPlayer.h"
#include <spdlog/spdlog.h>

namespace Ship {

SDLAudioPlayer::~SDLAudioPlayer() {
    SPDLOG_TRACE("destruct SDL audio player");
    if (mAudioStream) {
        SDL_DestroyAudioStream(mAudioStream);
        mAudioStream = nullptr;
    }
    SDL_QuitSubSystem(SDL_INIT_AUDIO);
}

bool SDLAudioPlayer::DoInit() {
    SPDLOG_INFO("[SDLAudioPlayer] DoInit() called");

    // On iOS, SDL audio subsystem may not be available or may require special permissions
    // For now, skip SDL_InitSubSystem and try to create the audio stream directly
    // since SDL is already initialized by the main application

    mNumChannels = this->GetAudioChannels() == AudioChannelsSetting::audioSurround51 ? 6 : 2;
    SPDLOG_INFO("[SDLAudioPlayer] Audio channels: {}", mNumChannels);

    // Create audio stream with source and destination specs
    SDL_AudioSpec spec;
    SDL_zero(spec);
    spec.freq = this->GetSampleRate();
    spec.format = SDL_AUDIO_S16;
    spec.channels = mNumChannels;

    SPDLOG_INFO("[SDLAudioPlayer] Audio spec - freq: {}, format: SDL_AUDIO_S16, channels: {}", spec.freq, spec.channels);

    // Create an audio stream
    SPDLOG_INFO("[SDLAudioPlayer] Calling SDL_OpenAudioDeviceStream...");
    mAudioStream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, NULL, NULL);
    if (!mAudioStream) {
        SPDLOG_ERROR("[SDLAudioPlayer] SDL_OpenAudioDeviceStream FAILED: {}", SDL_GetError());
        return false;
    }

    SPDLOG_INFO("[SDLAudioPlayer] SDL_OpenAudioDeviceStream succeeded, stream created: {}", (void*)mAudioStream);

    // Resume (unpause) the audio stream
    SPDLOG_INFO("[SDLAudioPlayer] Calling SDL_ResumeAudioStreamDevice...");
    SDL_ResumeAudioStreamDevice(mAudioStream);
    SPDLOG_INFO("[SDLAudioPlayer] Audio stream resumed and ready for playback");

    return true;
}

int SDLAudioPlayer::Buffered() {
    if (!mAudioStream) {
        return 0;
    }
    return SDL_GetAudioStreamAvailable(mAudioStream) / (sizeof(int16_t) * mNumChannels);
}

void SDLAudioPlayer::Play(const uint8_t* buf, size_t len) {
    if (!mAudioStream) {
        return;
    }
    if (Buffered() < 6000) {
        // Don't fill the audio buffer too much in case this happens
        SDL_PutAudioStreamData(mAudioStream, buf, len);
    }
}
} // namespace Ship
