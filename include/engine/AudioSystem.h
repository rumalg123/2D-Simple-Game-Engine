#pragma once

#include <cstddef>
#include <limits>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

using AudioClipHandle = std::size_t;

constexpr AudioClipHandle InvalidAudioClip = std::numeric_limits<AudioClipHandle>::max();

struct AudioClip {
    std::string name;
    std::string sourcePath;
    int sampleRate = 44100;
    int channels = 1;
    std::vector<short> samples;
};

class AudioSystem {
public:
    ~AudioSystem();

    AudioClipHandle loadWavFromFile(const std::string& name, const std::string& path);
    AudioClipHandle createTone(
        const std::string& name,
        float frequencyHz,
        float durationSeconds,
        float volume = 0.35f,
        int sampleRate = 44100);
    bool play(AudioClipHandle handle, float volume = 1.0f);
    void update();
    void stopAll();
    void clear();

    const AudioClip& getClip(AudioClipHandle handle) const;
    AudioClipHandle getClipHandle(const std::string& name) const;
    std::size_t clipCount() const;
    std::size_t activeVoiceCount() const;

private:
    struct ActiveVoice;

    std::vector<AudioClip> clips;
    std::unordered_map<std::string, AudioClipHandle> clipHandlesByName;
    std::vector<std::shared_ptr<ActiveVoice>> activeVoices;
};
