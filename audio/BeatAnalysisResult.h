#pragma once

#include <QVector>
#include <QString>

struct BeatEvent
{
    double timeSec = 0.0;
    float confidence = 1.0f;
    bool isDownbeat = false;

    bool operator<(const BeatEvent& other) const { return timeSec < other.timeSec; }
    bool operator<(double t) const { return timeSec < t; }
};

struct BeatAnalysisResult
{
    QVector<BeatEvent> beats;
    double bpm = 0.0;
    QString method;           // "aubio", "beatnet", "transformer"
    bool success = false;

    // Activation curve (beat probability per time frame)
    QVector<float> activation;   // Raw model output (beat class probability)
    double activationSampleRate = 0.0;  // Frames per second (activation fps)
    double activationOffset = 0.0;      // Time offset of first frame (seconds)

    // Downbeat activation (class 2 probability)
    QVector<float> downbeatActivation;

    // Candidate peaks before temporal decoding
    QVector<BeatEvent> candidateBeats;

    // External reference grid, e.g. exported from Ableton/Rekordbox/Serato.
    QVector<BeatEvent> referenceBeats;

    // Tempo diagnostics
    QVector<double> intervalHistogram;
    double histogramMinSec = 0.30;
    double histogramMaxSec = 1.20;
    double selectedTempoSec = 0;
    double selectedBpm = 0;
    double tempoScore = 0;
    int candidateCount = 0;
    double medianCandidateInterval = 0;
    double attackPhaseOffsetSec = 0;
    double perceptualShiftSec = 0;
    int attackPhaseMatches = 0;

    // Convenience accessors
    QVector<double> beatTimes() const
    {
        QVector<double> times;
        times.reserve(beats.size());
        for (const auto& b : beats)
            times.append(b.timeSec);
        return times;
    }

    int beatCount() const { return beats.size(); }

    // Convert frame index to time
    double frameToTime(int frame) const
    {
        if (activationSampleRate <= 0) return 0.0;
        return activationOffset + frame / activationSampleRate;
    }
};
