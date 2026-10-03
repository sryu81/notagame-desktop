#include "Audio.h"

#include "AndroidOut.h"
#include "Platform.h"

#define MINIAUDIO_IMPLEMENTATION
#define MA_NO_DECODING
#define MA_NO_ENCODING
#define MA_NO_GENERATION
#define MA_NO_RESOURCE_MANAGER
#define MA_NO_NODE_GRAPH
#define MA_NO_ENGINE
#include "miniaudio.h"

#include <algorithm>
#include <atomic>
#include <cstring>

struct Audio::Impl {
    const Platform *platform = nullptr;
    ma_device device{};
    bool deviceOpen = false;
    audio::State state;
    std::vector<const audio::Music *> playlist;
    std::mutex mutex;
    std::atomic<float> peak{0.0f};

    static void onData(ma_device *device, void *output, const void *, ma_uint32 frameCount) {
        auto *self = static_cast<Impl *>(device->pUserData);
        std::lock_guard<std::mutex> lock(self->mutex);
        auto *out = static_cast<float *>(output);
        audio::mix(self->state, out, static_cast<int>(frameCount));
        self->peak.store(audio::peakAbs(out, static_cast<int>(frameCount) * 2));
    }
};

Audio::Audio() : impl_(std::make_unique<Impl>()) {}

Audio::~Audio() { shutdown(); }

bool Audio::init(const Platform *platform) {
    impl_->platform = platform;

    ma_device_config config = ma_device_config_init(ma_device_type_playback);
    config.playback.format   = ma_format_f32;
    config.playback.channels = 2;
    config.sampleRate        = audio::kMixSampleRate;
    config.dataCallback      = &Impl::onData;
    config.pUserData         = impl_.get();
    config.performanceProfile = ma_performance_profile_low_latency;

    ma_result result = ma_device_init(nullptr, &config, &impl_->device);
    if (result != MA_SUCCESS) {
        aout << "[audio] ma_device_init failed: " << ma_result_description(result) << std::endl;
        return false;
    }
    impl_->deviceOpen = true;

    result = ma_device_start(&impl_->device);
    if (result != MA_SUCCESS) {
        aout << "[audio] ma_device_start failed: " << ma_result_description(result) << std::endl;
        ma_device_uninit(&impl_->device);
        impl_->deviceOpen = false;
        return false;
    }
    aout << "[audio] backend: " << ma_get_backend_name(impl_->device.pContext->backend) << std::endl;
    return true;
}

void Audio::shutdown() {
    if (impl_->deviceOpen) {
        ma_device_uninit(&impl_->device);
        impl_->deviceOpen = false;
    }
}

bool Audio::loadSound(const char *assetPath, audio::Sound &out) {
    if (!impl_->platform) return false;
    std::vector<uint8_t> bytes;
    if (!impl_->platform->readAsset(assetPath, bytes)) return false;
    return audio::loadSound(bytes.data(), bytes.size(), out);
}

bool Audio::loadMusic(const char *assetPath, audio::Music &out) {
    if (!impl_->platform) return false;
    std::vector<uint8_t> bytes;
    if (!impl_->platform->readAsset(assetPath, bytes)) return false;
    return audio::loadMusic(bytes.data(), bytes.size(), out);
}

void Audio::playSound(const audio::Sound &sound, float volume) {
    if (!impl_->deviceOpen) return;
    std::lock_guard<std::mutex> lock(impl_->mutex);
    int v = audio::allocateVoice(impl_->state);
    impl_->state.voices[v].sound = &sound;
    impl_->state.voices[v].position = 0;
    impl_->state.voices[v].volume = volume;
    impl_->state.voices[v].active = true;
}

void Audio::playMusic(const audio::Music &music, bool loop, float volume) {
    if (!impl_->deviceOpen) return;
    std::lock_guard<std::mutex> lock(impl_->mutex);
    impl_->state.music = &music;
    impl_->state.playlist = nullptr;
    impl_->state.playlistSize = 0;
    impl_->state.musicPosition = 0;
    impl_->state.musicLooping = loop;
    impl_->state.musicVolume = volume;
}

void Audio::stopMusic() {
    if (!impl_->deviceOpen) return;
    std::lock_guard<std::mutex> lock(impl_->mutex);
    impl_->state.music = nullptr;
    impl_->state.playlist = nullptr;
    impl_->state.playlistSize = 0;
    impl_->state.musicPosition = 0;
    impl_->state.musicLooping = false;
}

void Audio::setMasterVolume(float v) {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    impl_->state.masterVolume = v;
}

void Audio::setSfxVolume(float v) {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    impl_->state.sfxVolume = v;
}

void Audio::setMusicVolume(float v) {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    impl_->state.musicVolume = v;
}

float Audio::masterVolume() const {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    return impl_->state.masterVolume;
}

float Audio::sfxVolume() const {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    return impl_->state.sfxVolume;
}

float Audio::musicVolume() const {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    return impl_->state.musicVolume;
}

bool Audio::isOpen() const {
    return impl_->deviceOpen;
}

int Audio::activeVoiceCount() const {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    return audio::activeVoices(impl_->state);
}

const audio::Music *Audio::currentMusic() const {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    return impl_->state.music;
}

float Audio::outputPeak() const {
    return impl_->peak.load();
}

void Audio::playPlaylist(const std::vector<const audio::Music *> &tracks, float volume, int start) {
    if (!impl_->deviceOpen || tracks.empty()) return;
    std::lock_guard<std::mutex> lock(impl_->mutex);
    impl_->playlist = tracks;
    impl_->state.playlist = impl_->playlist.data();
    impl_->state.playlistSize = static_cast<int>(impl_->playlist.size());
    impl_->state.track = start % impl_->state.playlistSize;
    impl_->state.music = impl_->playlist[static_cast<size_t>(impl_->state.track)];
    impl_->state.musicPosition = 0;
    impl_->state.musicLooping = false;
    impl_->state.musicVolume = volume;
}

int Audio::currentTrack() const {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    return impl_->state.playlistSize > 0 ? impl_->state.track : -1;
}
