#include "TimelineWidget.h"
#include "TimelineTrack.h"
#include "TimelineViewState.h"
#include "../audio/TransportController.h"

#include <QPainter>
#include <QWheelEvent>
#include <QMouseEvent>
#include <algorithm>
#include <cmath>

namespace {
constexpr auto kBgDark        = QColor(18, 18, 24);
constexpr auto kBgPanel       = QColor(24, 24, 32);
constexpr auto kPlayheadColor = QColor(255, 255, 255, 200);
constexpr auto kTextColor     = QColor(160, 160, 180);
constexpr auto kTextDim       = QColor(100, 100, 120);
constexpr auto kGridColor     = QColor(40, 40, 55);
constexpr auto kBorderLine    = QColor(50, 50, 70);
} // namespace

TimelineWidget::TimelineWidget(QWidget* parent)
    : QWidget(parent)
{
    setMinimumHeight(200);
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);

    constexpr int kClickPoolSize = 8;
    m_clickSounds.reserve(kClickPoolSize);
    for (int i = 0; i < kClickPoolSize; ++i)
    {
        auto* sound = new QSoundEffect(this);
        sound->setSource(QUrl("qrc:/resources/metronome_click.wav"));
        sound->setVolume(0.8f);
        sound->setLoopCount(1);
        m_clickSounds.append(sound);
    }
}

TimelineWidget::~TimelineWidget() = default;

void TimelineWidget::setTransport(TransportController* transport) { m_transport = transport; }
void TimelineWidget::setViewState(TimelineViewState* state) { m_viewState = state; }

void TimelineWidget::addTrack(TimelineTrack* track)
{
    m_tracks.append(track);
    update();
}

void TimelineWidget::clearTracks()
{
    qDeleteAll(m_tracks);
    m_tracks.clear();
    update();
}

void TimelineWidget::setMetronomeEnabled(bool enabled)
{
    m_metronomeOn = enabled;
    if (enabled && !m_beatTimes.isEmpty())
    {
        const auto it = std::upper_bound(m_beatTimes.cbegin(), m_beatTimes.cend(), m_currentPositionSec);
        m_lastBeatIndex = static_cast<int>(std::distance(m_beatTimes.cbegin(), it)) - 1;
    }
    else
    {
        m_lastBeatIndex = -1;
    }
    update();
    emit metronomeToggled(enabled);
}

void TimelineWidget::setBeatTimes(const QVector<double>& times)
{
    m_beatTimes = times;
    std::sort(m_beatTimes.begin(), m_beatTimes.end());

    if (m_metronomeOn && !m_beatTimes.isEmpty())
    {
        const auto it = std::upper_bound(m_beatTimes.cbegin(), m_beatTimes.cend(), m_currentPositionSec);
        m_lastBeatIndex = static_cast<int>(std::distance(m_beatTimes.cbegin(), it)) - 1;
    }
    else
    {
        m_lastBeatIndex = -1;
    }
}

void TimelineWidget::playClickSound()
{
    if (m_clickSounds.isEmpty())
        return;

    for (int attempt = 0; attempt < m_clickSounds.size(); ++attempt)
    {
        const int index = (m_nextClickSound + attempt) % m_clickSounds.size();
        QSoundEffect* sound = m_clickSounds[index];
        if (sound && sound->status() == QSoundEffect::Ready && !sound->isPlaying())
        {
            sound->play();
            m_nextClickSound = (index + 1) % m_clickSounds.size();
            return;
        }
    }

    QSoundEffect* sound = m_clickSounds[m_nextClickSound % m_clickSounds.size()];
    if (sound && sound->status() == QSoundEffect::Ready)
    {
        sound->stop();
        sound->play();
        m_nextClickSound = (m_nextClickSound + 1) % m_clickSounds.size();
    }
}

void TimelineWidget::onPositionChanged(double sec)
{
    m_currentPositionSec = sec;
    updateBeatMetronome(sec);
    update();
}

void TimelineWidget::updateBeatMetronome(double timeSec)
{
    if (!m_metronomeOn || m_beatTimes.isEmpty()) return;

    const auto it = std::upper_bound(m_beatTimes.cbegin(), m_beatTimes.cend(), timeSec);
    const int idx = static_cast<int>(std::distance(m_beatTimes.cbegin(), it)) - 1;

    if (idx >= 0 && idx > m_lastBeatIndex)
    {
        m_lastBeatIndex = idx;
        playClickSound();
    }
    else if (idx < m_lastBeatIndex)
    {
        m_lastBeatIndex = idx;
    }
}

// ============================================================================
// Paint
// ============================================================================

void TimelineWidget::paintEvent(QPaintEvent* /*event*/)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    const QRect fullRect = rect();
    p.fillRect(fullRect, kBgDark);

    const double vs = m_viewState ? m_viewState->viewStart() : 0.0;
    const double vd = m_viewState ? m_viewState->viewDuration() : 10.0;
    const double ve = vs + vd;

    // Layout: [time ruler 22px] [tracks...]
    const int rulerH = 22;
    const int margin = 4;

    // Time ruler
    QRect rulerRect(margin, margin, fullRect.width() - margin * 2, rulerH);
    drawTimeRuler(p, rulerRect, vs, vd);

    // Metronome toggle button (right side of ruler)
    {
        int btnW = 60, btnH = 18;
        int btnX = rulerRect.right() - btnW - 4;
        int btnY = rulerRect.top() + (rulerRect.height() - btnH) / 2;
        m_metronomeBtnRect = QRect(btnX, btnY, btnW, btnH);

        if (m_metronomeOn)
        {
            p.fillRect(m_metronomeBtnRect, QColor(0, 180, 220, 100));
            p.setPen(QPen(QColor(0, 200, 240), 1));
        }
        else
        {
            p.setPen(kTextDim);
        }
        p.setFont(QFont("Segoe UI", 8));
        p.drawRect(m_metronomeBtnRect);
        p.drawText(m_metronomeBtnRect, Qt::AlignCenter, "Metronome");
    }

    // Stack tracks vertically
    int y = rulerRect.bottom() + margin;
    const int availableH = fullRect.height() - rulerH - margin * 2;

    for (auto& track : m_tracks)
    {
        if (!track) continue;

        int numTracks = std::max(1, static_cast<int>(m_tracks.size()));
        int trackH = std::min(track->preferredHeight(), std::max(track->minHeight(), availableH / numTracks));
        QRect trackRect(margin, y, fullRect.width() - margin * 2, trackH);

        p.save();
        p.setClipRect(trackRect);
        track->render(p, trackRect, *m_viewState, m_currentPositionSec);
        p.restore();

        // Track separator
        p.setPen(kBorderLine);
        p.drawLine(margin, trackRect.bottom(), fullRect.width() - margin, trackRect.bottom());

        y += trackH + margin;
    }

    // Playhead (drawn over all tracks)
    QRect playheadRect(margin, rulerRect.bottom() + margin,
                       fullRect.width() - margin * 2, y - rulerRect.bottom() - margin);
    drawPlayhead(p, playheadRect, vs, vd);
}

void TimelineWidget::drawPlayhead(QPainter& p, const QRect& rect, double vs, double vd)
{
    if (!m_viewState || vd <= 0) return;

    double ve = vs + vd;
    if (m_currentPositionSec < vs || m_currentPositionSec > ve) return;

    int x = rect.left() + static_cast<int>(((m_currentPositionSec - vs) / vd) * rect.width());

    // Playhead line (full height)
    p.setPen(QPen(kPlayheadColor, 2));
    p.drawLine(x, rect.top(), x, rect.bottom());

    // Playhead triangle at top
    QPolygonF tri;
    tri << QPointF(x - 5, rect.top()) << QPointF(x + 5, rect.top()) << QPointF(x, rect.top() + 7);
    p.setBrush(kPlayheadColor);
    p.setPen(Qt::NoPen);
    p.drawPolygon(tri);
}

void TimelineWidget::drawTimeRuler(QPainter& p, const QRect& rect, double vs, double vd)
{
    p.fillRect(rect, kBgPanel);
    p.setPen(kBorderLine);
    p.drawRect(rect);

    if (!m_viewState || vd <= 0) return;

    const int w = rect.width();
    const double ve = vs + vd;

    double tickInterval = 1.0;
    if (vd > 60) tickInterval = 10.0;
    else if (vd > 20) tickInterval = 5.0;
    else if (vd > 5) tickInterval = 2.0;
    else if (vd < 2) tickInterval = 0.5;

    double firstTick = std::ceil(vs / tickInterval) * tickInterval;

    p.setFont(QFont("Consolas", 8));
    for (double t = firstTick; t <= ve; t += tickInterval)
    {
        int x = rect.left() + static_cast<int>(((t - vs) / vd) * w);
        p.setPen(kGridColor);
        p.drawLine(x, rect.top(), x, rect.top() + 6);
        p.setPen(kTextColor);
        QString label = (tickInterval >= 1.0) ? QStringLiteral("%1s").arg(static_cast<int>(t))
                                              : QStringLiteral("%1").arg(t, 0, 'f', 1);
        p.drawText(x - 15, rect.top() + 7, 30, 15, Qt::AlignCenter, label);
    }
}

// ============================================================================
// Input
// ============================================================================

void TimelineWidget::wheelEvent(QWheelEvent* event)
{
    if (!m_viewState) return;

    QPoint numDegrees = event->angleDelta() / 120;
    if (numDegrees.isNull()) return;

    double zoomFactor = (numDegrees.y() > 0) ? 0.8 : 1.25;
    double cursorFrac = std::clamp(static_cast<double>(event->position().x() - 4) / (width() - 8), 0.0, 1.0);

    m_viewState->zoom(zoomFactor, cursorFrac);
    update();
    event->accept();
}

void TimelineWidget::mousePressEvent(QMouseEvent* event)
{
    // Metronome button click
    if (m_metronomeBtnRect.contains(event->pos()))
    {
        setMetronomeEnabled(!m_metronomeOn);
        update();
        return;
    }

    QWidget::mousePressEvent(event);
}
