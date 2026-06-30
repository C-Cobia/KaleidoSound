#pragma once

#include <QPainter>
#include <QRect>

class TimelineViewState;
struct AudioData;
struct SpectrumFrame;

class TimelineTrack
{
public:
    virtual ~TimelineTrack() = default;

    // Render this track within its allocated rectangle
    virtual void render(
        QPainter& painter,
        const QRect& trackRect,
        const TimelineViewState& viewState,
        double playheadSec) = 0;

    // Preferred height in pixels
    virtual int preferredHeight() const = 0;

    // Minimum height
    virtual int minHeight() const { return 40; }

    // Track name (for debugging / future track headers)
    virtual const char* name() const = 0;

    // Whether this track has data to render
    virtual bool hasData() const = 0;
};
