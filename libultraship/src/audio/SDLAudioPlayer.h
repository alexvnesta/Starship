#pragma once
#include "AudioPlayer.h"
#include <SDL3/SDL.h>

namespace Ship {
class SDLAudioPlayer final : public AudioPlayer {
  public:
    SDLAudioPlayer(AudioSettings settings) : AudioPlayer(settings) {
    }
    ~SDLAudioPlayer();

    int Buffered();
    void Play(const uint8_t* buf, size_t len);

  protected:
    bool DoInit();

  private:
    SDL_AudioStream* mAudioStream = nullptr;
    int32_t mNumChannels = 2;
};
} // namespace Ship
