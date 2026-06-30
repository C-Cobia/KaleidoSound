#pragma once

#include <QObject>
#include <QScrollBar>
#include <algorithm>

class TransportController;

class TimelineViewState : public QObject
{
    Q_OBJECT

public:
    explicit TimelineViewState(QObject* parent = nullptr);

    void setTransport(TransportController* transport);
    void setTotalDuration(double sec);

    // Viewport
    double viewStart() const { return m_viewStartSec; }
    double viewDuration() const { return m_viewDurationSec; }
    double viewEnd() const { return m_viewStartSec + m_viewDurationSec; }
    double totalDuration() const { return m_totalDurationSec; }

    // Time <-> pixel mapping (for a given widget width)
    double timeToPixel(double timeSec, int widgetWidth) const;
    double pixelToTime(int pixelX, int widgetWidth) const;

    // Viewport manipulation
    void setViewStart(double sec);
    void setViewDuration(double dur);
    void zoom(double factor, double centerFraction = 0.5);
    void scrollTo(double centerTimeSec);
    void fitToContent();

    // Soft follow: update viewport based on playhead position
    void updateFollow(double playheadSec);

    bool isFollowing() const { return m_followEnabled; }
    void setFollowEnabled(bool enabled) { m_followEnabled = enabled; }

signals:
    void viewportChanged();
    void totalDurationChanged(double sec);

private:
    TransportController* m_transport = nullptr;
    double m_viewStartSec = 0.0;
    double m_viewDurationSec = 10.0;
    double m_totalDurationSec = 0.0;
    bool m_followEnabled = true;
};
