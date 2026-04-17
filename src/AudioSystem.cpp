#include "AudioSystem.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <mmsystem.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <memory>
#include <stdexcept>
#include <utility>

namespace {
constexpr float Pi = 3.14159265358979323846f;

std::uint16_t readU16(const std::vector<unsigned char>& data, std::size_t offset) {
    if (offset + 2 > data.size()) {
        throw std::runtime_error("Unexpected end of WAV file.");
    }

    return static_cast<std::uint16_t>(data[offset] | (data[offset + 1] << 8));
}

std::uint32_t readU32(const std::vector<unsigned char>& data, std::size_t offset) {
    if (offset + 4 > data.size()) {
        throw std::runtime_error("Unexpected end of WAV file.");
    }

    return static_cast<std::uint32_t>(
        static_cast<std::uint32_t>(data[offset]) |
        (static_cast<std::uint32_t>(data[offset + 1]) << 8) |
        (static_cast<std::uint32_t>(data[offset + 2]) << 16) |
        (static_cast<std::uint32_t>(data[offset + 3]) << 24));
}

bool hasFourCc(const std::vector<unsigned char>& data, std::size_t offset, const char* expected) {
    if (offset + 4 > data.size()) {
        return false;
    }

    return data[offset] == static_cast<unsigned char>(expected[0]) &&
           data[offset + 1] == static_cast<unsigned char>(expected[1]) &&
           data[offset + 2] == static_cast<unsigned char>(expected[2]) &&
           data[offset + 3] == static_cast<unsigned char>(expected[3]);
}

short clampToI16(float value) {
    value = std::max(-1.0f, std::min(1.0f, value));
    return static_cast<short>(value * 32767.0f);
}

AudioClip loadWavClip(const std::string& name, const std::string& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        throw std::runtime_error("Failed to open WAV file: " + path);
    }

    std::vector<unsigned char> data(
        (std::istreambuf_iterator<char>(input)),
        std::istreambuf_iterator<char>());

    if (data.size() < 12 || !hasFourCc(data, 0, "RIFF") || !hasFourCc(data, 8, "WAVE")) {
        throw std::runtime_error("Invalid WAV file: " + path);
    }

    bool foundFormat = false;
    bool foundData = false;
    std::uint16_t audioFormat = 0;
    std::uint16_t channels = 0;
    std::uint32_t sampleRate = 0;
    std::uint16_t bitsPerSample = 0;
    std::size_t dataOffset = 0;
    std::uint32_t dataSize = 0;

    std::size_t offset = 12;
    while (offset + 8 <= data.size()) {
        const std::uint32_t chunkSize = readU32(data, offset + 4);
        const std::size_t chunkData = offset + 8;
        if (chunkData + chunkSize > data.size()) {
            throw std::runtime_error("Invalid WAV chunk size: " + path);
        }

        if (hasFourCc(data, offset, "fmt ")) {
            if (chunkSize < 16) {
                throw std::runtime_error("Invalid WAV format chunk: " + path);
            }

            audioFormat = readU16(data, chunkData);
            channels = readU16(data, chunkData + 2);
            sampleRate = readU32(data, chunkData + 4);
            bitsPerSample = readU16(data, chunkData + 14);
            foundFormat = true;
        } else if (hasFourCc(data, offset, "data")) {
            dataOffset = chunkData;
            dataSize = chunkSize;
            foundData = true;
        }

        offset = chunkData + chunkSize + (chunkSize % 2);
    }

    if (!foundFormat || !foundData) {
        throw std::runtime_error("WAV file is missing fmt or data chunk: " + path);
    }
    if (audioFormat != 1) {
        throw std::runtime_error("Only PCM WAV files are supported: " + path);
    }
    if (channels != 1 && channels != 2) {
        throw std::runtime_error("Only mono and stereo WAV files are supported: " + path);
    }
    if (bitsPerSample != 8 && bitsPerSample != 16) {
        throw std::runtime_error("Only 8-bit and 16-bit WAV files are supported: " + path);
    }

    AudioClip clip;
    clip.name = name;
    clip.sourcePath = path;
    clip.sampleRate = static_cast<int>(sampleRate);
    clip.channels = static_cast<int>(channels);

    if (bitsPerSample == 8) {
        clip.samples.reserve(dataSize);
        for (std::uint32_t index = 0; index < dataSize; ++index) {
            const int centered = static_cast<int>(data[dataOffset + index]) - 128;
            clip.samples.push_back(static_cast<short>(centered << 8));
        }
    } else {
        if (dataSize % 2 != 0) {
            throw std::runtime_error("Invalid 16-bit WAV data size: " + path);
        }

        clip.samples.reserve(dataSize / 2);
        for (std::uint32_t index = 0; index < dataSize; index += 2) {
            const std::uint16_t sample = readU16(data, dataOffset + index);
            clip.samples.push_back(static_cast<short>(static_cast<std::int16_t>(sample)));
        }
    }

    return clip;
}
}

struct AudioSystem::ActiveVoice {
    HWAVEOUT device = nullptr;
    WAVEHDR header{};
    std::vector<short> samples;
    bool prepared = false;
};

AudioSystem::~AudioSystem() {
    clear();
}

AudioClipHandle AudioSystem::loadWavFromFile(const std::string& name, const std::string& path) {
    const auto existing = clipHandlesByName.find(name);
    if (existing != clipHandlesByName.end()) {
        return existing->second;
    }

    AudioClip clip = loadWavClip(name, path);
    const AudioClipHandle handle = clips.size();
    clips.push_back(std::move(clip));
    clipHandlesByName[name] = handle;
    return handle;
}

AudioClipHandle AudioSystem::createTone(
    const std::string& name,
    float frequencyHz,
    float durationSeconds,
    float volume,
    int sampleRate) {
    const auto existing = clipHandlesByName.find(name);
    if (existing != clipHandlesByName.end()) {
        return existing->second;
    }

    sampleRate = std::max(8000, sampleRate);
    durationSeconds = std::max(0.01f, durationSeconds);
    frequencyHz = std::max(20.0f, frequencyHz);
    volume = std::max(0.0f, std::min(1.0f, volume));

    const int sampleCount = std::max(1, static_cast<int>(durationSeconds * static_cast<float>(sampleRate)));
    AudioClip clip;
    clip.name = name;
    clip.sampleRate = sampleRate;
    clip.channels = 1;
    clip.samples.reserve(sampleCount);

    for (int index = 0; index < sampleCount; ++index) {
        const float t = static_cast<float>(index) / static_cast<float>(sampleRate);
        const float envelope = 1.0f - (static_cast<float>(index) / static_cast<float>(sampleCount));
        clip.samples.push_back(clampToI16(std::sin(2.0f * Pi * frequencyHz * t) * volume * envelope));
    }

    const AudioClipHandle handle = clips.size();
    clips.push_back(std::move(clip));
    clipHandlesByName[name] = handle;
    return handle;
}

bool AudioSystem::play(AudioClipHandle handle, float volume) {
    if (handle >= clips.size()) {
        return false;
    }

    update();

    const AudioClip& clip = clips[handle];
    if (clip.samples.empty()) {
        return false;
    }

    volume = std::max(0.0f, std::min(1.0f, volume));

    auto voice = std::make_shared<ActiveVoice>();
    voice->samples.resize(clip.samples.size());
    for (std::size_t index = 0; index < clip.samples.size(); ++index) {
        voice->samples[index] = clampToI16(static_cast<float>(clip.samples[index]) / 32767.0f * volume);
    }

    WAVEFORMATEX format{};
    format.wFormatTag = WAVE_FORMAT_PCM;
    format.nChannels = static_cast<WORD>(clip.channels);
    format.nSamplesPerSec = static_cast<DWORD>(clip.sampleRate);
    format.wBitsPerSample = 16;
    format.nBlockAlign = static_cast<WORD>(format.nChannels * (format.wBitsPerSample / 8));
    format.nAvgBytesPerSec = format.nSamplesPerSec * format.nBlockAlign;

    MMRESULT result = waveOutOpen(&voice->device, WAVE_MAPPER, &format, 0, 0, CALLBACK_NULL);
    if (result != MMSYSERR_NOERROR) {
        return false;
    }

    voice->header.lpData = reinterpret_cast<LPSTR>(voice->samples.data());
    voice->header.dwBufferLength = static_cast<DWORD>(voice->samples.size() * sizeof(short));
    result = waveOutPrepareHeader(voice->device, &voice->header, sizeof(WAVEHDR));
    if (result != MMSYSERR_NOERROR) {
        waveOutClose(voice->device);
        return false;
    }
    voice->prepared = true;

    result = waveOutWrite(voice->device, &voice->header, sizeof(WAVEHDR));
    if (result != MMSYSERR_NOERROR) {
        waveOutUnprepareHeader(voice->device, &voice->header, sizeof(WAVEHDR));
        waveOutClose(voice->device);
        return false;
    }

    activeVoices.push_back(std::move(voice));
    return true;
}

void AudioSystem::update() {
    auto voice = activeVoices.begin();
    while (voice != activeVoices.end()) {
        if (((*voice)->header.dwFlags & WHDR_DONE) == 0) {
            ++voice;
            continue;
        }

        if ((*voice)->prepared) {
            waveOutUnprepareHeader((*voice)->device, &(*voice)->header, sizeof(WAVEHDR));
            (*voice)->prepared = false;
        }
        if ((*voice)->device) {
            waveOutClose((*voice)->device);
            (*voice)->device = nullptr;
        }

        voice = activeVoices.erase(voice);
    }
}

void AudioSystem::stopAll() {
    for (std::shared_ptr<ActiveVoice>& voice : activeVoices) {
        if (voice->device) {
            waveOutReset(voice->device);
        }
        if (voice->prepared) {
            waveOutUnprepareHeader(voice->device, &voice->header, sizeof(WAVEHDR));
            voice->prepared = false;
        }
        if (voice->device) {
            waveOutClose(voice->device);
            voice->device = nullptr;
        }
    }

    activeVoices.clear();
}

void AudioSystem::clear() {
    stopAll();
    clips.clear();
    clipHandlesByName.clear();
}

const AudioClip& AudioSystem::getClip(AudioClipHandle handle) const {
    if (handle >= clips.size()) {
        throw std::out_of_range("Invalid audio clip handle");
    }

    return clips[handle];
}

AudioClipHandle AudioSystem::getClipHandle(const std::string& name) const {
    const auto existing = clipHandlesByName.find(name);
    return existing != clipHandlesByName.end() ? existing->second : InvalidAudioClip;
}

std::size_t AudioSystem::clipCount() const {
    return clips.size();
}

std::size_t AudioSystem::activeVoiceCount() const {
    return activeVoices.size();
}
