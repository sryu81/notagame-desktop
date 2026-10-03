#ifndef NOT_A_GAME_AUDIO_H
#define NOT_A_GAME_AUDIO_H

#include "WavLoader.h"

#include <cstdint>
#include <memory>
#include <mutex>
#include <vector>

class Platform;

namespace audio {

constexpr int kMaxSfxVoices = 8;
constexpr uint32_t kMixSampleRate = 48000;

struct Sound {
    std::vector<float> samples; // mono, kMixSampleRate
};

struct Music {
    std::vector<float> samples; // mono, kMixSampleRate
};

struct Voice {
    const Sound *sound = nullptr;
    size_t position = 0;
    float volume = 1.0f;
    bool active = false;
};

struct State {
    Voice voices[kMaxSfxVoices];
    const Music *music = nullptr;
    const Music *const *playlist = nullptr;
    int playlistSize = 0;
    int track = 0;
    size_t musicPosition = 0;
    bool musicLooping = false;
    float musicVolume = 1.0f;
    float sfxVolume = 1.0f;
    float masterVolume = 1.0f;
};

void mix(State &state, float *outInterleavedStereo, int numFrames);

int allocateVoice(State &state);
int activeVoices(const State &state);
float peakAbs(const float *samples, int count);

bool loadSound(const uint8_t *bytes, size_t size, Sound &out);
bool loadMusic(const uint8_t *bytes, size_t size, Music &out);

void selfTest();

} // namespace audio

class Audio {
public:
    Audio();
    ~Audio();

    Audio(const Audio &) = delete;
    Audio &operator=(const Audio &) = delete;

    bool init(const Platform *platform);
    void shutdown();

    bool loadSound(const char *assetPath, audio::Sound &out);
    bool loadMusic(const char *assetPath, audio::Music &out);

    void playSound(const audio::Sound &sound, float volume = 1.0f);
    void playMusic(const audio::Music &music, bool loop = true, float volume = 1.0f);
    void playPlaylist(const std::vector<const audio::Music *> &tracks, float volume = 1.0f, int start = 0);
    int currentTrack() const;
    void stopMusic();

    void setMasterVolume(float v);
    void setSfxVolume(float v);
    void setMusicVolume(float v);

    float masterVolume() const;
    float sfxVolume() const;
    float musicVolume() const;

    bool isOpen() const;
    int activeVoiceCount() const;
    const audio::Music *currentMusic() const;
    float outputPeak() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

#endif
