#include "BeatTrack.h"
#include "TimelineViewState.h"

#include <QPainter>
#include <algorithm>
#include <cmath>

namespace {
constexpr auto kBgBeat         = QColor(20, 20, 28);
constexpr auto kBeatColor      = QColor(255, 200, 60, 180);
constexpr auto kBeatHighlight  = QColor(255, 100, 60);
constexpr auto kDownbeatColor  = QColor(255, 80, 120);
constexpr auto kIntervalGood   = QColor(80, 220, 150);
constexpr auto kIntervalWarn   = QColor(255, 190, 70);
constexpr auto kIntervalBad    = QColor(255, 80, 90);
constexpr auto kBorderLine     = QColor(50, 50, 70);
constexpr auto kActivationHigh = QColor(0, 200, 255);
constexpr auto kDownbeatHeat   = QColor(255, 80, 150);
constexpr auto kCandidateColor = QColor(150, 155, 170, 105);
constexpr auto kReferenceColor = QColor(80, 255, 170, 190);
} // namespace

BeatTrack::BeatTrack() = default;

void BeatTrack::setBeatData(const BeatAnalysisResult& beats)
{
    m_beats = beats;
    m_hasData = !beats.beats.isEmpty() || !beats.activation.isEmpty();
}

void BeatTrack::clear()
{
    m_beats = BeatAnalysisResult{};
    m_hasData = false;
    m_highlightIndex = -1;
}

void BeatTrack::render(QPainter& painter, const QRect& trackRect,
                       const TimelineViewState& viewState, double playheadSec)
{
    painter.fillRect(trackRect, kBgBeat);
    painter.setPen(kBorderLine);
    painter.drawRect(trackRect);

    if (!m_hasData) return;

    const int w = trackRect.width();
    const double vs = viewState.viewStart();
    const double vd = viewState.viewDuration();
    const double ve = vs + vd;

    const QRect activationRect = trackRect.adjusted(0, 16, 0, -trackRect.height() / 3);
    const QRect intervalRect(trackRect.left(), activationRect.bottom() + 1,
                             trackRect.width(), trackRect.bottom() - activationRect.bottom());

    painter.setPen(QPen(QColor(40, 40, 55), 1));
    painter.drawLine(trackRect.left(), intervalRect.top(), trackRect.right(), intervalRect.top());

    float actMax = 0.0f;
    for (float v : m_beats.activation)
        actMax = std::max(actMax, v);

    float downMax = 0.0f;
    for (float v : m_beats.downbeatActivation)
        downMax = std::max(downMax, v);

    // --- Draw beat and downbeat activation heat curves ---
    if (!m_beats.activation.isEmpty() && m_beats.activationSampleRate > 0 && actMax > 0.0f)
    {
        const int actFrames = m_beats.activation.size();
        const double actFps = m_beats.activationSampleRate;

        for (int px = 0; px < w; ++px)
        {
            double timeSec = vs + (static_cast<double>(px) / w) * vd;
            int actFrame = static_cast<int>((timeSec - m_beats.activationOffset) * actFps);

            if (actFrame < 0 || actFrame >= actFrames) continue;

            const float actValue = std::clamp(m_beats.activation[actFrame] / actMax, 0.0f, 1.0f);

            if (actValue < 0.01f) continue;

            int r = static_cast<int>(actValue * kActivationHigh.red());
            int g = static_cast<int>(actValue * kActivationHigh.green());
            int b = static_cast<int>(40 + actValue * (kActivationHigh.blue() - 40));
            int a = static_cast<int>(30 + actValue * 150);

            QColor color(r, g, b, a);
            int barH = static_cast<int>(actValue * (activationRect.height() - 2));
            int x = trackRect.left() + px;
            painter.fillRect(x, activationRect.bottom() - barH, 1, barH, color);
        }
    }

    if (!m_beats.downbeatActivation.isEmpty() && m_beats.activationSampleRate > 0 && downMax > 0.0f)
    {
        const int actFrames = m_beats.downbeatActivation.size();
        const double actFps = m_beats.activationSampleRate;

        for (int px = 0; px < w; ++px)
        {
            double timeSec = vs + (static_cast<double>(px) / w) * vd;
            int actFrame = static_cast<int>((timeSec - m_beats.activationOffset) * actFps);

            if (actFrame < 0 || actFrame >= actFrames) continue;

            const float value = std::clamp(m_beats.downbeatActivation[actFrame] / downMax, 0.0f, 1.0f);
            if (value < 0.01f) continue;

            QColor color = kDownbeatHeat;
            color.setAlpha(static_cast<int>(35 + value * 160));
            const int barH = std::max(1, static_cast<int>(value * activationRect.height() * 0.55f));
            const int x = trackRect.left() + px;
            painter.fillRect(x, activationRect.top(), 1, barH, color);
        }
    }

    // --- Draw inter-beat interval jitter ---
    QVector<double> visibleIntervals;
    for (int i = 1; i < m_beats.beats.size(); ++i)
    {
        const double t0 = m_beats.beats[i - 1].timeSec;
        const double t1 = m_beats.beats[i].timeSec;
        if (t1 < vs || t0 > ve) continue;
        const double interval = t1 - t0;
        if (interval > 0.1 && interval < 3.0)
            visibleIntervals.append(interval);
    }

    double medianInterval = 0.0;
    if (!visibleIntervals.isEmpty())
    {
        std::sort(visibleIntervals.begin(), visibleIntervals.end());
        medianInterval = visibleIntervals[visibleIntervals.size() / 2];
    }

    if (medianInterval > 0.0)
    {
        painter.setPen(QPen(QColor(80, 80, 100), 1, Qt::DashLine));
        painter.drawLine(intervalRect.left(), intervalRect.center().y(),
                         intervalRect.right(), intervalRect.center().y());

        QPointF previousPoint;
        bool hasPrevious = false;
        for (int i = 1; i < m_beats.beats.size(); ++i)
        {
            const double t0 = m_beats.beats[i - 1].timeSec;
            const double t1 = m_beats.beats[i].timeSec;
            const double mid = (t0 + t1) * 0.5;
            if (mid < vs || mid > ve) continue;

            const double interval = t1 - t0;
            if (interval <= 0.1 || interval >= 3.0) continue;

            const double deviation = interval - medianInterval;
            const double normalized = std::clamp(deviation / std::max(0.08, medianInterval * 0.25), -1.0, 1.0);
            const int x = trackRect.left() + static_cast<int>(((mid - vs) / vd) * w);
            const int y = intervalRect.center().y() - static_cast<int>(normalized * intervalRect.height() * 0.42);

            QColor dotColor = kIntervalGood;
            if (std::abs(deviation) > 0.06)
                dotColor = kIntervalBad;
            else if (std::abs(deviation) > 0.025)
                dotColor = kIntervalWarn;

            QPointF point(x, y);
            if (hasPrevious)
            {
                painter.setPen(QPen(QColor(120, 120, 145, 110), 1));
                painter.drawLine(previousPoint, point);
            }
            painter.setPen(Qt::NoPen);
            painter.setBrush(dotColor);
            painter.drawEllipse(point, 2.5, 2.5);

            if (vd < 12.0)
            {
                painter.setPen(QColor(170, 170, 190, 170));
                painter.setFont(QFont("Consolas", 7));
                painter.drawText(QRect(x - 22, y - 13, 44, 10), Qt::AlignCenter,
                                 QStringLiteral("%1").arg(interval * 1000.0, 0, 'f', 0));
            }

            previousPoint = point;
            hasPrevious = true;
        }
    }

    QVector<double> phaseErrors;
    if (!m_beats.referenceBeats.isEmpty() && !m_beats.beats.isEmpty())
    {
        for (const auto& reference : m_beats.referenceBeats)
        {
            if (reference.timeSec < vs || reference.timeSec > ve) continue;

            const int x = trackRect.left() + static_cast<int>(((reference.timeSec - vs) / vd) * w);
            painter.setPen(QPen(kReferenceColor, reference.isDownbeat ? 2.0 : 1.2));
            painter.drawLine(x, trackRect.top() + 1, x, trackRect.bottom() - 1);
        }

        for (const auto& beat : m_beats.beats)
        {
            double bestError = 0.25;
            bool matched = false;
            for (const auto& reference : m_beats.referenceBeats)
            {
                const double err = beat.timeSec - reference.timeSec;
                if (std::abs(err) < std::abs(bestError))
                {
                    bestError = err;
                    matched = true;
                }
            }
            if (matched)
                phaseErrors.append(bestError);
        }
    }

    // --- Draw candidate peaks before temporal decoding ---
    for (const auto& candidate : m_beats.candidateBeats)
    {
        if (candidate.timeSec < vs || candidate.timeSec > ve) continue;

        const int x = trackRect.left() + static_cast<int>(((candidate.timeSec - vs) / vd) * w);
        const float conf = std::clamp(candidate.confidence, 0.0f, 1.0f);
        QColor color = kCandidateColor;
        color.setAlpha(static_cast<int>(45 + conf * 110));
        painter.setPen(QPen(color, 1, Qt::DashLine));
        painter.drawLine(x, activationRect.top(), x, intervalRect.bottom());
    }

    // --- Draw beat markers over the debug data ---
    for (int i = 0; i < m_beats.beats.size(); ++i)
    {
        const auto& beat = m_beats.beats[i];
        if (beat.timeSec < vs || beat.timeSec > ve) continue;

        int x = trackRect.left() + static_cast<int>(((beat.timeSec - vs) / vd) * w);

        bool isHighlighted = (i == m_highlightIndex) ||
                             (std::abs(beat.timeSec - playheadSec) < vd * 0.01);

        if (beat.isDownbeat)
        {
            // Downbeat: red, full height
            painter.setPen(QPen(kDownbeatColor, 2));
            int markerH = trackRect.height() - 2;
            painter.drawLine(x, trackRect.top() + 1, x, trackRect.top() + 1 + markerH);
        }
        else if (isHighlighted)
        {
            // Current beat: bright, tall
            painter.setPen(QPen(kBeatHighlight, 2));
            int markerH = trackRect.height() - 4;
            painter.drawLine(x, trackRect.top() + 2, x, trackRect.top() + 2 + markerH);
        }
        else
        {
            // Normal beat: confidence-weighted alpha and height
            float conf = std::clamp(beat.confidence, 0.0f, 1.0f);
            int alpha = static_cast<int>(80 + conf * 175);
            int markerH = static_cast<int>(trackRect.height() * (0.3f + conf * 0.7f));

            QColor beatCol = kBeatColor;
            beatCol.setAlpha(alpha);
            painter.setPen(QPen(beatCol, 1.0f + conf));

            int yTop = trackRect.bottom() - markerH;
            painter.drawLine(x, yTop, x, trackRect.bottom());
        }
    }

    // --- Method + BPM label ---
    painter.setPen(kBeatColor);
    painter.setFont(QFont("Consolas", 9, QFont::Bold));
    QString label = QStringLiteral("BPM %1  beats %2")
        .arg(m_beats.bpm, 0, 'f', 1)
        .arg(m_beats.beats.size());
    if (!m_beats.method.isEmpty())
        label += QStringLiteral(" [%1]").arg(m_beats.method);
    if (actMax > 0.0f)
        label += QStringLiteral("  act %1").arg(actMax, 0, 'f', 3);
    if (!m_beats.candidateBeats.isEmpty())
        label += QStringLiteral("  cand %1").arg(m_beats.candidateBeats.size());
    if (medianInterval > 0.0)
        label += QStringLiteral("  IBI %1ms").arg(medianInterval * 1000.0, 0, 'f', 0);
    if (!phaseErrors.isEmpty())
    {
        std::sort(phaseErrors.begin(), phaseErrors.end());
        const double medianError = phaseErrors[phaseErrors.size() / 2];
        double sum = 0.0;
        for (double err : phaseErrors)
            sum += (err - medianError) * (err - medianError);
        const double stdMs = std::sqrt(sum / phaseErrors.size()) * 1000.0;
        label += QStringLiteral("  ref %1ms/%2ms")
            .arg(medianError * 1000.0, 0, 'f', 0)
            .arg(stdMs, 0, 'f', 0);
    }
    painter.drawText(trackRect.adjusted(8, 4, 0, 0), Qt::AlignTop | Qt::AlignLeft, label);
}
