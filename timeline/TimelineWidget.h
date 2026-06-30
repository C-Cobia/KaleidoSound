#pragma once

#include <QWidget>
#include <QVector>
#include <QSoundEffect>
#include <QUrl>
#include <memory>

class TimelineTrack;
class TransportController;
class TimelineViewState;

class TimelineWidget : public QWidget
{
    Q_OBJECT

public:
    explicit TimelineWidget(QWidget* parent = nullptr);
    ~TimelineWidget() override;

    void setTransport(TransportController* transport);
    void setViewState(TimelineViewState* state);

    // Track management (takes ownership)
    void addTrack(TimelineTrack* track);
    void clearTracks();

    // Metronome
    void setMetronomeEnabled(bool enabled);
    bool isMetronomeEnabled() const { return m_metronomeOn; }
    void setBeatTimes(const QVector<double>& times);

    // Called by TransportController when position changes
    void onPositionChanged(double sec);

signals:
    void metronomeToggled(bool enabled);

protected:
    void paintEvent(QPaintEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;

private:
    void drawPlayhead(QPainter& p, const QRect& rect, double vs, double vd);
    void drawTimeRuler(QPainter& p, const QRect& rect, double vs, double vd);
    void playClickSound();
    void updateBeatMetronome(double timeSec);

    TransportController* m_transport = nullptr;
    TimelineViewState* m_viewState = nullptr;

    QVector<TimelineTrack*> m_tracks;

    double m_currentPositionSec = 0.0;

    // Metronome
    bool m_metronomeOn = false;
    int m_lastBeatIndex = -1;
    QVector<QSoundEffect*> m_clickSounds;
    int m_nextClickSound = 0;

    // Beat data (for metronome)
    QVector<double> m_beatTimes;

    // Metronome button rect
    QRect m_metronomeBtnRect;
};
