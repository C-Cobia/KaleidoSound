#pragma once

#include "IBeatDetector.h"

class AubioBeatDetector : public IBeatDetector
{
public:
    BeatAnalysisResult analyze(const AudioData& audio) override;
    const char* name() const override { return "aubio"; }
    bool isRealtime() const override { return true; }
};
