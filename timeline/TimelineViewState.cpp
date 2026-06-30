#include "TimelineViewState.h"
#include "../audio/TransportController.h"

TimelineViewState::TimelineViewState(QObject* parent)
    : QObject(parent)
{
}

void TimelineViewState::setTransport(TransportController* transport)
{
    m_transport = transport;
}

void TimelineViewState::setTotalDuration(double sec)
{
    if (std::abs(m_totalDurationSec - sec) < 0.001) return;
    m_totalDurationSec = sec;
    fitToContent();
    emit totalDurationChanged(sec);
}

// ============================================================================
// Time <-> Pixel
// ============================================================================

double TimelineViewState::timeToPixel(double timeSec, int widgetWidth) const
{
    if (m_viewDurationSec <= 0 || widgetWidth <= 0) return 0;
    return ((timeSec - m_viewStartSec) / m_viewDurationSec) * widgetWidth;
}

double TimelineViewState::pixelToTime(int pixelX, int widgetWidth) const
{
    if (widgetWidth <= 0) return m_viewStartSec;
    return m_viewStartSec + (static_cast<double>(pixelX) / widgetWidth) * m_viewDurationSec;
}

// ============================================================================
// Viewport manipulation
// ============================================================================

void TimelineViewState::setViewStart(double sec)
{
    double maxStart = std::max(0.0, m_totalDurationSec - m_viewDurationSec);
    sec = std::clamp(sec, 0.0, maxStart);
    if (std::abs(m_viewStartSec - sec) < 0.0001) return;
    m_viewStartSec = sec;
    emit viewportChanged();
}

void TimelineViewState::setViewDuration(double dur)
{
    dur = std::clamp(dur, 0.1, std::max(1.0, m_totalDurationSec));
    if (std::abs(m_viewDurationSec - dur) < 0.0001) return;
    m_viewDurationSec = dur;
    // Clamp start after duration change
    double maxStart = std::max(0.0, m_totalDurationSec - m_viewDurationSec);
    m_viewStartSec = std::clamp(m_viewStartSec, 0.0, maxStart);
    emit viewportChanged();
}

void TimelineViewState::zoom(double factor, double centerFraction)
{
    centerFraction = std::clamp(centerFraction, 0.0, 1.0);

    double centerTime = m_viewStartSec + centerFraction * m_viewDurationSec;

    m_viewDurationSec *= factor;
    m_viewDurationSec = std::clamp(m_viewDurationSec, 0.1, std::max(1.0, m_totalDurationSec));

    m_viewStartSec = centerTime - centerFraction * m_viewDurationSec;

    double maxStart = std::max(0.0, m_totalDurationSec - m_viewDurationSec);
    m_viewStartSec = std::clamp(m_viewStartSec, 0.0, maxStart);

    emit viewportChanged();
}

void TimelineViewState::scrollTo(double centerTimeSec)
{
    m_viewStartSec = centerTimeSec - m_viewDurationSec * 0.5;
    double maxStart = std::max(0.0, m_totalDurationSec - m_viewDurationSec);
    m_viewStartSec = std::clamp(m_viewStartSec, 0.0, maxStart);
    emit viewportChanged();
}

void TimelineViewState::fitToContent()
{
    m_viewStartSec = 0.0;
    m_viewDurationSec = std::max(1.0, m_totalDurationSec);
    emit viewportChanged();
}

// ============================================================================
// Soft Follow
// ============================================================================

void TimelineViewState::updateFollow(double playheadSec)
{
    if (!m_followEnabled || m_viewDurationSec <= 0) return;

    double relPos = (playheadSec - m_viewStartSec) / m_viewDurationSec;

    // Only scroll when playhead is outside 30%-70% zone
    if (relPos > 0.7)
    {
        m_viewStartSec = playheadSec - m_viewDurationSec * 0.5;
    }
    else if (relPos < 0.3)
    {
        m_viewStartSec = playheadSec - m_viewDurationSec * 0.5;
    }
    else
    {
        return;  // Playhead is in the safe zone, no scroll needed
    }

    double maxStart = std::max(0.0, m_totalDurationSec - m_viewDurationSec);
    m_viewStartSec = std::clamp(m_viewStartSec, 0.0, maxStart);
    emit viewportChanged();
}
