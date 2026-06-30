#pragma once

#include "TimelineTrack.h"
#include "../audio/AudioAnalyzer.h"
#include <QVector>

class WaveformTrack : public TimelineTrack
{
public:
    WaveformTrack();

    void setAudioData(const AudioData& data);
    void clear();

    // TimelineTrack interface
    void render(QPainter& painter, const QRect& trackRect,
                const TimelineViewState& viewState, double playheadSec) override;
    int preferredHeight() const override { return 120; }
    const char* name() const override { return "Waveform"; }
    bool hasData() const override { return m_hasData; }

private:
    void computeDownsample(int displayWidth);
    void drawWaveform(QPainter& p, const QRect& rect, double vs, double vd);

    AudioData m_audioData;
    bool m_hasData = false;

    // Downsampled waveform for display
    QVector<float> m_waveformMax;
    QVector<float> m_waveformMin;
    int m_lastComputeWidth = 0;
};
