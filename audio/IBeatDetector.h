#pragma once

#include "BeatAnalysisResult.h"
#include "AudioAnalyzer.h"

class IBeatDetector
{
public:
    virtual ~IBeatDetector() = default;

    virtual BeatAnalysisResult analyze(const AudioData& audio) = 0;
    virtual const char* name() const = 0;
    virtual bool isAvailable() const { return true; }

    // Is this detector suitable for real-time use?
    virtual bool isRealtime() const { return false; }
};
