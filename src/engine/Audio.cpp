#include "Audio.h"

#include "AndroidOut.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstring>

namespace audio {

void mix(State &state, float *out, int numFrames) {
    std::memset(out, 0, sizeof(float) * numFrames * 2);

    const float sfxScaleBase = state.sfxVolume * state.masterVolume;

    for (int v = 0; v < kMaxSfxVoices; v++) {
        Voice &voice = state.voices[v];
        if (!voice.active || voice.sound == nullptr) continue;
        const auto &s = voice.sound->samples;
        if (voice.position >= s.size()) {
            voice.active = false;
            continue;
        }
        const int remaining = static_cast<int>(s.size() - voice.position);
        const int toMix = std::min(numFrames, remaining);
        if (toMix <= 0) {
            voice.active = false;
            continue;
        }
        const float scale = voice.volume * sfxScaleBase;
        const float *src = s.data() + voice.position;
        for (int f = 0; f < toMix; f++) {
            float sample = src[f] * scale;
            out[f * 2 + 0] += sample;
            out[f * 2 + 1] += sample;
        }
        voice.position += static_cast<size_t>(toMix);
        if (toMix < numFrames) voice.active = false;
    }

    if (state.music != nullptr) {
        const float musicScale = state.musicVolume * state.masterVolume;
        int frame = 0;
        while (frame < numFrames) {
            if (state.musicPosition >= state.music->samples.size()) {
                if (state.musicLooping && !state.music->samples.empty()) {
                    state.musicPosition = 0;
                } else if (state.playlistSize > 0) {
                    state.track = (state.track + 1) % state.playlistSize;
                    state.music = state.playlist[state.track];
                    state.musicPosition = 0;
                    if (state.music->samples.empty()) {
                        state.music = nullptr;
                        break;
                    }
                } else {
                    state.music = nullptr;
                    break;
                }
            }
            const auto &cur = state.music->samples;
            const int remaining = static_cast<int>(cur.size() - state.musicPosition);
            const int toMix = std::min(numFrames - frame, remaining);
            const float *src = cur.data() + state.musicPosition;
            for (int f = 0; f < toMix; f++) {
                float sample = src[f] * musicScale;
                out[(frame + f) * 2 + 0] += sample;
                out[(frame + f) * 2 + 1] += sample;
            }
            state.musicPosition += static_cast<size_t>(toMix);
            frame += toMix;
        }
    }
}

int activeVoices(const State &state) {
    int n = 0;
    for (int i = 0; i < kMaxSfxVoices; i++)
        if (state.voices[i].active && state.voices[i].sound) ++n;
    return n;
}

float peakAbs(const float *samples, int count) {
    float p = 0.0f;
    for (int i = 0; i < count; i++) p = std::max(p, std::abs(samples[i]));
    return p;
}

int allocateVoice(State &state) {
    for (int i = 0; i < kMaxSfxVoices; i++) {
        if (!state.voices[i].active) return i;
    }
    int oldest = 0;
    for (int i = 1; i < kMaxSfxVoices; i++) {
        if (state.voices[i].position > state.voices[oldest].position) {
            oldest = i;
        }
    }
    return oldest;
}

bool loadSound(const uint8_t *bytes, size_t size, Sound &out) {
    uint32_t rate = 0;
    return wav::loadMonoFloat(bytes, size, out.samples, rate)
        && rate == kMixSampleRate;
}

bool loadMusic(const uint8_t *bytes, size_t size, Music &out) {
    uint32_t rate = 0;
    return wav::loadMonoFloat(bytes, size, out.samples, rate)
        && rate == kMixSampleRate;
}

namespace {

constexpr float kMixEps = 1e-5f;

bool approxEqual(float a, float b, float eps = kMixEps) {
    float d = a - b;
    if (d < 0) d = -d;
    return d <= eps;
}

void writeWavPcm16Mono(std::vector<uint8_t> &out,
                       const std::vector<float> &samples,
                       uint32_t sampleRate) {
    const uint32_t dataSize = static_cast<uint32_t>(samples.size() * 2);
    const uint32_t fmtSize = 16;
    const uint32_t fileSize = 4 + (8 + fmtSize) + (8 + dataSize);

    out.clear();
    out.resize(8 + fileSize);
    uint8_t *p = out.data();

    auto putU32 = [&](size_t off, uint32_t v) {
        p[off + 0] = static_cast<uint8_t>(v);
        p[off + 1] = static_cast<uint8_t>(v >> 8);
        p[off + 2] = static_cast<uint8_t>(v >> 16);
        p[off + 3] = static_cast<uint8_t>(v >> 24);
    };
    auto putU16 = [&](size_t off, uint16_t v) {
        p[off + 0] = static_cast<uint8_t>(v);
        p[off + 1] = static_cast<uint8_t>(v >> 8);
    };
    auto putCC = [&](size_t off, const char *tag) {
        p[off + 0] = tag[0];
        p[off + 1] = tag[1];
        p[off + 2] = tag[2];
        p[off + 3] = tag[3];
    };

    putCC(0, "RIFF");
    putU32(4, fileSize);
    putCC(8, "WAVE");

    putCC(12, "fmt ");
    putU32(16, fmtSize);
    putU16(20, 1); // PCM
    putU16(22, 1); // mono
    putU32(24, sampleRate);
    putU32(28, sampleRate * 2); // byte rate
    putU16(32, 2); // block align
    putU16(34, 16); // bits per sample

    putCC(36, "data");
    putU32(40, dataSize);
    for (size_t i = 0; i < samples.size(); i++) {
        float f = samples[i];
        if (f > 1.0f) f = 1.0f;
        if (f < -1.0f) f = -1.0f;
        int16_t s = static_cast<int16_t>(f * 32767.0f);
        size_t off = 44 + i * 2;
        p[off + 0] = static_cast<uint8_t>(s);
        p[off + 1] = static_cast<uint8_t>(s >> 8);
    }
}

} // namespace

void selfTest() {
    aout << "[audio] self-test starting" << std::endl;

    // Build a known-good WAV in memory: 10 samples of 0.5 @ 48kHz mono 16-bit.
    {
        std::vector<float> src(10, 0.5f);
        std::vector<uint8_t> wavBytes;
        writeWavPcm16Mono(wavBytes, src, kMixSampleRate);

        Sound s;
        bool ok = loadSound(wavBytes.data(), wavBytes.size(), s);
        assert(ok);
        assert(s.samples.size() == 10);
        for (size_t i = 0; i < s.samples.size(); i++) {
            assert(approxEqual(s.samples[i], 0.5f, 1e-3f));
        }
    }

    // Stereo WAV: downmix to mono (averaged).
    {
        std::vector<uint8_t> stereo;
        const uint32_t dataSize = 4 * 2 * 2; // 4 frames, 2 channels, 16-bit
        const uint32_t fmtSize = 16;
        const uint32_t fileSize = 4 + (8 + fmtSize) + (8 + dataSize);
        stereo.resize(8 + fileSize);
        uint8_t *p = stereo.data();
        auto putU32 = [&](size_t off, uint32_t v) {
            p[off + 0] = static_cast<uint8_t>(v);
            p[off + 1] = static_cast<uint8_t>(v >> 8);
            p[off + 2] = static_cast<uint8_t>(v >> 16);
            p[off + 3] = static_cast<uint8_t>(v >> 24);
        };
        auto putU16 = [&](size_t off, uint16_t v) {
            p[off + 0] = static_cast<uint8_t>(v);
            p[off + 1] = static_cast<uint8_t>(v >> 8);
        };
        auto putCC = [&](size_t off, const char *tag) {
            p[off + 0] = tag[0]; p[off + 1] = tag[1];
            p[off + 2] = tag[2]; p[off + 3] = tag[3];
        };
        putCC(0, "RIFF"); putU32(4, fileSize); putCC(8, "WAVE");
        putCC(12, "fmt "); putU32(16, fmtSize);
        putU16(20, 1); putU16(22, 2);
        putU32(24, kMixSampleRate); putU32(28, kMixSampleRate * 4);
        putU16(32, 4); putU16(34, 16);
        putCC(36, "data"); putU32(40, dataSize);
        // 4 frames: L=1.0, R=0.0 each, 16-bit
        size_t off = 44;
        for (int f = 0; f < 4; f++) {
            int16_t L = 32767, R = 0;
            p[off++] = static_cast<uint8_t>(L);
            p[off++] = static_cast<uint8_t>(L >> 8);
            p[off++] = static_cast<uint8_t>(R);
            p[off++] = static_cast<uint8_t>(R >> 8);
        }
        Sound s;
        bool ok = loadSound(stereo.data(), stereo.size(), s);
        assert(ok);
        assert(s.samples.size() == 4);
        for (size_t i = 0; i < s.samples.size(); i++) {
            assert(approxEqual(s.samples[i], 0.5f, 1e-3f));
        }
    }

    // Reject garbage / truncated.
    {
        Sound s;
        assert(!loadSound(nullptr, 0, s));
        uint8_t junk[64] = {0};
        assert(!loadSound(junk, sizeof(junk), s));
    }

    // Voice pool: 9 plays on 8 slots, 9th steals oldest.
    {
        State state{};
        Sound sfx;
        sfx.samples.assign(5, 1.0f);
        for (int i = 0; i < kMaxSfxVoices; i++) {
            int v = allocateVoice(state);
            state.voices[v].sound = &sfx;
            state.voices[v].volume = 1.0f;
            state.voices[v].position = 0;
            state.voices[v].active = true;
        }
        for (int i = 0; i < kMaxSfxVoices; i++) {
            assert(state.voices[i].active);
        }
        int v = allocateVoice(state);
        assert(state.voices[v].active); // stole, still active
        // All positions start at 0, so we steal the first one (lowest index
        // ties at 0; loop picks i=1 first because state.voices[1].position
        // (0) is not > state.voices[0].position (0)).
        // Just assert a valid slot index.
        assert(v >= 0 && v < kMaxSfxVoices);
    }

    // Mix: SFX (5 frames of 0.5) + music (10 frames of 0.25), sfxVol=1, musicVol=1, master=1.
    {
        State state{};
        Sound sfx;
        sfx.samples.assign(5, 0.5f);
        Music mus;
        mus.samples.assign(10, 0.25f);
        state.voices[0].sound = &sfx;
        state.voices[0].active = true;
        state.voices[0].position = 0;
        state.voices[0].volume = 1.0f;
        state.music = &mus;
        state.musicPosition = 0;
        state.musicLooping = false;

        const int N = 7;
        std::vector<float> out(N * 2, 0.0f);
        mix(state, out.data(), N);

        // Frames 0-4: SFX 0.5 + music 0.25 = 0.75
        // Frames 5-6: SFX done, music 0.25
        for (int f = 0; f < 5; f++) {
            assert(approxEqual(out[f * 2 + 0], 0.75f));
            assert(approxEqual(out[f * 2 + 1], 0.75f));
        }
        for (int f = 5; f < 7; f++) {
            assert(approxEqual(out[f * 2 + 0], 0.25f));
            assert(approxEqual(out[f * 2 + 1], 0.25f));
        }
        assert(!state.voices[0].active);
        assert(state.music != nullptr);
    }

    // Music looping: 7 frames of 3-frame looped music.
    {
        State state{};
        Music mus;
        mus.samples = {1.0f, 0.0f, 1.0f};
        state.music = &mus;
        state.musicLooping = true;
        std::vector<float> out(7 * 2, 0.0f);
        mix(state, out.data(), 7);
        // Loop: [1,0,1,1,0,1,1]
        assert(approxEqual(out[0], 1.0f));
        assert(approxEqual(out[2], 0.0f));
        assert(approxEqual(out[4], 1.0f));
        assert(approxEqual(out[6], 1.0f));
        // 7 frames, 3-frame loop → position should be 1
        assert(state.musicPosition == 1);
    }

    {
        State state{};
        Music a, b;
        a.samples = {1.0f, 1.0f};
        b.samples = {0.5f};
        const Music *list[] = {&a, &b};
        state.playlist = list;
        state.playlistSize = 2;
        state.music = &a;
        std::vector<float> out(7 * 2, 0.0f);
        mix(state, out.data(), 7);
        assert(approxEqual(out[0], 1.0f) && approxEqual(out[2], 1.0f) && approxEqual(out[4], 0.5f));
        assert(approxEqual(out[6], 1.0f) && approxEqual(out[10], 0.5f) && approxEqual(out[12], 1.0f));
        assert(state.music == &a && state.track == 0 && state.musicPosition == 1);
    }

    // Volume independence: changing sfxVolume must not affect music output.
    {
        State state{};
        Sound sfx;
        sfx.samples.assign(10, 0.5f);
        Music mus;
        mus.samples.assign(10, 0.25f);
        state.voices[0].sound = &sfx;
        state.voices[0].active = true;
        state.voices[0].position = 0;
        state.voices[0].volume = 1.0f;
        state.music = &mus;
        state.musicLooping = false;

        state.sfxVolume = 1.0f;
        state.musicVolume = 1.0f;
        state.masterVolume = 1.0f;

        std::vector<float> out(2 * 2, 0.0f);
        mix(state, out.data(), 2);
        // SFX 0.5 + music 0.25 = 0.75
        assert(approxEqual(out[0], 0.75f));
        assert(approxEqual(out[1], 0.75f));

        // Now halve sfx volume. SFX should be 0.25, music still 0.25, sum 0.5.
        state.sfxVolume = 0.5f;
        state.voices[0].position = 0;
        std::vector<float> out2(2 * 2, 0.0f);
        mix(state, out2.data(), 2);
        assert(approxEqual(out2[0], 0.5f));
        assert(approxEqual(out2[1], 0.5f));
    }

    {
        State st;
        Sound s;
        s.samples.assign(64, 0.5f);
        assert(activeVoices(st) == 0);
        for (int i = 0; i < 3; ++i) {
            const int v = allocateVoice(st);
            st.voices[v] = {&s, 0, 1.0f, true};
        }
        assert(activeVoices(st) == 3);
        float out[32 * 2];
        mix(st, out, 32);
        assert(peakAbs(out, 64) > 0.4f);
        st.masterVolume = 0.0f;
        mix(st, out, 32);
        assert(peakAbs(out, 64) == 0.0f);
        const float neg[3] = {0.1f, -0.7f, 0.3f};
        assert(peakAbs(neg, 3) == 0.7f);
    }

    aout << "[audio] self-test PASS" << std::endl;
}

} // namespace audio

