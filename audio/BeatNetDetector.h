#pragma once

#include "IBeatDetector.h"

class BeatDetector;

class BeatNetDetector : public IBeatDetector
{
public:
    BeatNetDetector();
    ~BeatNetDetector() override;

    BeatAnalysisResult analyze(const AudioData& audio) override;
    const char* name() const override { return "beatnet"; }
    bool isRealtime() const override { return false; }
    bool isAvailable() const override;

    bool loadModel(const QString& modelPath);

    // Feature dump for validation
    bool dumpFeatures(const AudioData& data, const QString& outputPath);

private:
    BeatDetector* m_detector = nullptr;
};
