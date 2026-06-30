#pragma once

#include <QObject>
#include <QVector>
#include <QString>
#include <cstdint>
#include "BeatAnalysisResult.h"

struct AudioData
{
    QVector<float> samples;
    int sampleRate = 44100;
    int channels = 1;
    double durationSec = 0.0;
};

struct SpectrumFrame
{
    QVector<float> magnitudes;
};

// Legacy compatibility (used by BeatDetector internally)
struct BeatInfo
{
    QVector<double> beatTimes;
    QVector<bool> beatIsDownbeat;
    QVector<float> beatConfidences;
    QVector<double> candidatePeakTimes;
    QVector<float> candidatePeakScores;
    double bpm = 0.0;
    QVector<float> activation;        // Raw beat probability curve
    QVector<float> downbeatActivation;
    double activationSampleRate = 0;  // Frames per second

    // Tempo diagnostics (populated by decoder)
    QVector<double> intervalHistogram;    // IBI histogram values
    double histogramMinSec = 0.30;        // Min interval in histogram
    double histogramMaxSec = 1.20;        // Max interval in histogram
    double selectedTempoSec = 0;          // Selected IBI (seconds)
    double selectedBpm = 0;              // Selected BPM
    double tempoScore = 0;              // Confidence of tempo selection
    int candidateCount = 0;             // Number of candidate peaks
    double medianCandidateInterval = 0; // Median IBI among candidates
    double attackPhaseOffsetSec = 0;    // Median high-frequency attack offset
    double perceptualShiftSec = 0;      // Extra listening-oriented pre-shift
    int attackPhaseMatches = 0;         // Beats used for attack offset estimate
};

class IBeatDetector;
class AnalysisManager;

class AudioAnalyzer : public QObject
{
    Q_OBJECT

public:
    explicit AudioAnalyzer(QObject* parent = nullptr);
    ~AudioAnalyzer() override;

    bool loadFile(const QString& path, QString* error = nullptr);
    BeatAnalysisResult detectBeats(const AudioData& data);
    SpectrumFrame computeSpectrum(const AudioData& data, int startSample, int fftSize);

    const AudioData& audioData() const { return m_data; }

    void setAnalysisManager(AnalysisManager* manager) { m_analysisManager = manager; }

private:
    bool loadWav(const QString& path, QString* error = nullptr);
    bool loadWithQtDecoder(const QString& path, QString* error = nullptr);

    AudioData m_data;
    AnalysisManager* m_analysisManager = nullptr;
};
