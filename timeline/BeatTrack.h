#pragma once

#include "TimelineTrack.h"
#include "../audio/BeatAnalysisResult.h"

class BeatTrack : public TimelineTrack
{
public:
    BeatTrack();

    void setBeatData(const BeatAnalysisResult& beats);
    void setHighlightIndex(int index) { m_highlightIndex = index; }
    void clear();

    // TimelineTrack interface
    void render(QPainter& painter, const QRect& trackRect,
                const TimelineViewState& viewState, double playheadSec) override;
    int preferredHeight() const override { return 96; }
    int minHeight() const override { return 72; }
    const char* name() const override { return "Beat Debug"; }
    bool hasData() const override { return m_hasData; }

private:
    BeatAnalysisResult m_beats;
    bool m_hasData = false;
    int m_highlightIndex = -1;
};
