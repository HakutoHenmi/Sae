#include <iostream>
#include <fstream>
#include <vector>
#include <cmath>
#include <string>

const int SAMPLE_RATE = 44100;
const int MAX_AMP = 32767;

#pragma pack(push, 1)
struct WavHeader {
    char chunkId[4] = {'R', 'I', 'F', 'F'};
    uint32_t chunkSize = 0;
    char format[4] = {'W', 'A', 'V', 'E'};
    char subchunk1Id[4] = {'f', 'm', 't', ' '};
    uint32_t subchunk1Size = 16;
    uint16_t audioFormat = 1;
    uint16_t numChannels = 1;
    uint32_t sampleRate = SAMPLE_RATE;
    uint32_t byteRate = SAMPLE_RATE * 2;
    uint16_t blockAlign = 2;
    uint16_t bitsPerSample = 16;
    char subchunk2Id[4] = {'d', 'a', 't', 'a'};
    uint32_t subchunk2Size = 0;
};
#pragma pack(pop)

void SaveWav(const std::string& filename, const std::vector<int16_t>& data) {
    WavHeader header;
    header.subchunk2Size = data.size() * sizeof(int16_t);
    header.chunkSize = 36 + header.subchunk2Size;

    std::ofstream out(filename, std::ios::binary);
    if (!out) {
        std::cerr << "Failed to open " << filename << std::endl;
        return;
    }
    out.write(reinterpret_cast<const char*>(&header), sizeof(header));
    out.write(reinterpret_cast<const char*>(data.data()), header.subchunk2Size);
    out.close();
}

std::vector<int16_t> GenerateSendSound() {
    int numSamples = SAMPLE_RATE * 1.5;
    std::vector<int16_t> data(numSamples);
    float freq = 880.0f;
    for (int i = 0; i < numSamples; ++i) {
        float t = (float)i / SAMPLE_RATE;
        float env = 1.0f;
        float attack = 0.01f;
        if (t < attack) env = t / attack;
        else env = std::exp(-(t - attack) * 4.0f);
        
        float val = std::sin(2.0f * 3.1415926f * freq * t) * env;
        val += 0.3f * std::sin(2.0f * 3.1415926f * (freq * 2) * t) * env;
        val *= 0.4f;
        data[i] = (int16_t)(val * MAX_AMP);
    }
    return data;
}

std::vector<int16_t> GenerateZenSound() {
    int numSamples = SAMPLE_RATE * 2.0;
    std::vector<int16_t> data(numSamples);
    for (int i = 0; i < numSamples; ++i) {
        float t = (float)i / SAMPLE_RATE;
        float duration = 2.0f;
        float env = 1.0f;
        if (t < 0.5f) env = t / 0.5f;
        else if (t > duration - 0.5f) env = (duration - t) / 0.5f;
        
        float freq = 150.0f + 50.0f * std::sin(2.0f * 3.1415926f * 0.2f * t);
        float val = std::sin(2.0f * 3.1415926f * freq * t) * env;
        data[i] = (int16_t)(val * 0.2f * MAX_AMP);
    }
    return data;
}

std::vector<int16_t> GenerateFocusFinishSound() {
    int numSamples = SAMPLE_RATE * 3.0;
    std::vector<int16_t> data(numSamples);
    float freqs[] = {523.25f, 659.25f, 783.99f, 1046.50f};
    for (int i = 0; i < numSamples; ++i) {
        float t = (float)i / SAMPLE_RATE;
        float attack = 0.05f;
        float env = 1.0f;
        if (t < attack) env = t / attack;
        else env = std::exp(-(t - attack) * 2.0f);
        
        float val = 0.0f;
        for (float f : freqs) {
            val += std::sin(2.0f * 3.1415926f * f * t) * env;
        }
        val = (val / 4.0f) * 0.5f;
        data[i] = (int16_t)(val * MAX_AMP);
    }
    return data;
}

std::vector<int16_t> GenerateHitSound() {
    int numSamples = SAMPLE_RATE * 0.2;
    std::vector<int16_t> data(numSamples);
    for (int i = 0; i < numSamples; ++i) {
        float t = (float)i / SAMPLE_RATE;
        float freq = 1200.0f * std::exp(-t * 30.0f);
        float env = std::exp(-t * 20.0f);
        float val = std::sin(2.0f * 3.1415926f * freq * t) * env;
        data[i] = (int16_t)(val * 0.15f * MAX_AMP);
    }
    return data;
}

int main() {
    // std::filesystem::create_directories("Assets/Sounds");
    // Not using C++17 filesystem just to be safe, assume Assets/Sounds exists
    system("mkdir \"Assets\\Sounds\" 2> nul");
    SaveWav("Assets/Sounds/send.wav", GenerateSendSound());
    SaveWav("Assets/Sounds/zen_mode.wav", GenerateZenSound());
    SaveWav("Assets/Sounds/focus_finish.wav", GenerateFocusFinishSound());
    SaveWav("Assets/Sounds/hit.wav", GenerateHitSound());
    std::cout << "Sounds generated in Assets/Sounds" << std::endl;
    return 0;
}
