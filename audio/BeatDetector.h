#pragma once

#include <QString>
#include <QVector>

#include "AudioAnalyzer.h"

// Forward declare to avoid including torch headers here (Qt conflict)
namespace torch { namespace jit { class Module; } }

class BeatDetector
{
public:
    BeatDetector();
    ~BeatDetector();

    bool loadModel(const QString& modelPath);
    bool isModelLoaded() const { return m_modelLoaded; }

    BeatInfo detect(const AudioData& data);

    // Dump features to binary file for validation (float32, raw)
    bool dumpFeatures(const AudioData& data, const QString& outputPath);

    // Dump all intermediate stages for validation
    bool dumpStages(const AudioData& data, const QString& outputDir);

    // Get raw features (for validation)
    std::vector<std::vector<float>> getFeatures(const AudioData& data);

private:
    // Log-spectrogram feature extraction (BeatNet compatible)
    std::vector<std::vector<float>> computeLogSpectrogram(
        const float* samples, int numSamples, int sampleRate);

    // Build linear bandpass filterbank
    void buildFilterbank(int fftSize, int sampleRate, int numBands);

    // Beat tracking from model activations
    BeatInfo trackBeats(
        const std::vector<float>& activations,
        const std::vector<float>& downbeatActivations,
        double sampleRate, int hopSize, double durationSec);

    void alignBeatsToTransients(BeatInfo& info, const QVector<float>& audio, int sampleRate);

    // Apply Hanning window
    static std::vector<float> hanningWindow(int size);

    // Simple FFT (radix-2, in-place)
    static void fft(std::vector<float>& real, std::vector<float>& imag);

    void* m_module = nullptr; // torch::jit::script::Module (opaque pointer)
    bool m_modelLoaded = false;

    // Linear filterbank for BeatNet: [numBands x fftSize/2+1]
    QVector<QVector<float>> m_filterbank;
    int m_fftSize = 2048;   // FFT size for BeatNet log-spectrogram features
    int m_hopSize = 441;    // 20ms at 22050 Hz
    int m_featureDim = 272; // 136 log bands + 136 positive temporal differences
    int m_sampleRate = 22050;
};
