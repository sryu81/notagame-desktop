#include "Audio.h"

#include "AndroidOut.h"

#include <oboe/Oboe.h>

#include <android/asset_manager.h>

#include <algorithm>
#include <cstring>

struct Audio::Impl {
    AAssetManager *assetManager = nullptr;
    std::shared_ptr<oboe::AudioStream> stream;
    std::shared_ptr<oboe::AudioStreamDataCallback> callback;
    audio::State state;
    std::mutex mutex;

    class Callback : public oboe::AudioStreamDataCallback {
    public:
        Callback(Impl *owner) : owner_(owner) {}
        oboe::DataCallbackResult onAudioReady(
            oboe::AudioStream *,
            void *audioData,
            int32_t numFrames) override {
            std::lock_guard<std::mutex> lock(owner_->mutex);
            audio::mix(owner_->state,
                       static_cast<float *>(audioData),
                       static_cast<int>(numFrames));
            return oboe::DataCallbackResult::Continue;
        }
    private:
        Impl *owner_;
    };
};

Audio::Audio() : impl_(std::make_unique<Impl>()) {}

Audio::~Audio() { shutdown(); }

bool Audio::init(AAssetManager *assetManager) {
    impl_->assetManager = assetManager;
    impl_->callback = std::make_shared<Impl::Callback>(impl_.get());

    oboe::AudioStreamBuilder builder;
    builder.setDirection(oboe::Direction::Output)
           ->setPerformanceMode(oboe::PerformanceMode::LowLatency)
           ->setSharingMode(oboe::SharingMode::Exclusive)
           ->setFormat(oboe::AudioFormat::Float)
           ->setChannelCount(oboe::ChannelCount::Stereo)
           ->setSampleRate(audio::kMixSampleRate)
           ->setDataCallback(impl_->callback);

    auto result = builder.openStream(impl_->stream);
    if (result != oboe::Result::OK || !impl_->stream) {
        aout << "[audio] openStream failed: " << oboe::convertToText(result) << std::endl;
        return false;
    }

    auto startResult = impl_->stream->requestStart();
    if (startResult != oboe::Result::OK) {
        aout << "[audio] requestStart failed: " << oboe::convertToText(startResult) << std::endl;
        impl_->stream->close();
        impl_->stream.reset();
        return false;
    }
    return true;
}

void Audio::shutdown() {
    if (impl_->stream) {
        impl_->stream->requestStop();
        impl_->stream->close();
        impl_->stream.reset();
    }
}

bool Audio::loadSound(const char *assetPath, audio::Sound &out) {
    if (!impl_->assetManager) return false;
    AAsset *asset = AAssetManager_open(impl_->assetManager, assetPath, AASSET_MODE_BUFFER);
    if (!asset) return false;
    const uint8_t *bytes = static_cast<const uint8_t *>(AAsset_getBuffer(asset));
    size_t size = static_cast<size_t>(AAsset_getLength(asset));
    bool ok = audio::loadSound(bytes, size, out);
    AAsset_close(asset);
    return ok;
}

bool Audio::loadMusic(const char *assetPath, audio::Music &out) {
    if (!impl_->assetManager) return false;
    AAsset *asset = AAssetManager_open(impl_->assetManager, assetPath, AASSET_MODE_BUFFER);
    if (!asset) return false;
    const uint8_t *bytes = static_cast<const uint8_t *>(AAsset_getBuffer(asset));
    size_t size = static_cast<size_t>(AAsset_getLength(asset));
    bool ok = audio::loadMusic(bytes, size, out);
    AAsset_close(asset);
    return ok;
}

void Audio::playSound(const audio::Sound &sound, float volume) {
    if (!impl_->stream) return;
    std::lock_guard<std::mutex> lock(impl_->mutex);
    int v = audio::allocateVoice(impl_->state);
    impl_->state.voices[v].sound = &sound;
    impl_->state.voices[v].position = 0;
    impl_->state.voices[v].volume = volume;
    impl_->state.voices[v].active = true;
}

void Audio::playMusic(const audio::Music &music, bool loop, float volume) {
    if (!impl_->stream) return;
    std::lock_guard<std::mutex> lock(impl_->mutex);
    impl_->state.music = &music;
    impl_->state.musicPosition = 0;
    impl_->state.musicLooping = loop;
    impl_->state.musicVolume = volume;
}

void Audio::stopMusic() {
    if (!impl_->stream) return;
    std::lock_guard<std::mutex> lock(impl_->mutex);
    impl_->state.music = nullptr;
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
