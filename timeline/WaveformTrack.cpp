#include "WaveformTrack.h"
#include "TimelineViewState.h"

#include <QPainter>
#include <QPainterPath>
#include <algorithm>
#include <cmath>

namespace {
constexpr auto kBgWaveform   = QColor(14, 14, 20);
constexpr auto kWaveformFill = QColor(0, 180, 220, 60);
constexpr auto kWaveformLine = QColor(0, 200, 240);
constexpr auto kGridColor    = QColor(40, 40, 55);
constexpr auto kBorderLine   = QColor(50, 50, 70);
} // namespace

WaveformTrack::WaveformTrack() = default;

void WaveformTrack::setAudioData(const AudioData& data)
{
    m_audioData = data;
    m_hasData = !data.samples.isEmpty();
    m_lastComputeWidth = 0;  // Force recompute
    computeDownsample(1200);
}

void WaveformTrack::clear()
{
    m_audioData = AudioData{};
    m_hasData = false;
    m_waveformMax.clear();
    m_waveformMin.clear();
    m_lastComputeWidth = 0;
}

void WaveformTrack::computeDownsample(int displayWidth)
{
    const int totalSamples = m_audioData.samples.size();
    if (totalSamples == 0 || displayWidth == 0) return;

    m_waveformMax.resize(displayWidth);
    m_waveformMin.resize(displayWidth);

    const int samplesPerPixel = std::max(1, totalSamples / displayWidth);
    for (int px = 0; px < displayWidth; ++px)
    {
        int start = px * samplesPerPixel;
        int end = std::min(start + samplesPerPixel, totalSamples);
        float maxVal = -1.0f, minVal = 1.0f;
        for (int i = start; i < end; ++i)
        {
            float s = m_audioData.samples[i];
            maxVal = std::max(maxVal, s);
            minVal = std::min(minVal, s);
        }
        m_waveformMax[px] = maxVal;
        m_waveformMin[px] = minVal;
    }
}

void WaveformTrack::render(QPainter& painter, const QRect& trackRect,
                           const TimelineViewState& viewState, double /*playheadSec*/)
{
    painter.fillRect(trackRect, kBgWaveform);
    painter.setPen(kBorderLine);
    painter.drawRect(trackRect);

    if (!m_hasData) return;

    // Recompute downsample if width changed
    if (trackRect.width() != m_lastComputeWidth)
    {
        m_lastComputeWidth = trackRect.width();
        computeDownsample(trackRect.width());
    }

    drawWaveform(painter, trackRect, viewState.viewStart(), viewState.viewDuration());
}

void WaveformTrack::drawWaveform(QPainter& p, const QRect& rect, double vs, double vd)
{
    const int w = rect.width();
    const int centerY = rect.center().y();

    // Center line
    p.setPen(QPen(kGridColor, 1, Qt::DashLine));
    p.drawLine(rect.left(), centerY, rect.right(), centerY);

    if (m_audioData.samples.isEmpty()) return;

    const double startSample = vs * m_audioData.sampleRate;
    const double samplesInView = vd * m_audioData.sampleRate;
    const double samplesPerPixel = samplesInView / w;
    const int totalSamples = m_audioData.samples.size();

    QPainterPath pathTop, pathBottom;
    bool first = true;

    for (int px = 0; px < w; ++px)
    {
        int s0 = std::clamp(static_cast<int>(std::floor(startSample + px * samplesPerPixel)), 0, totalSamples - 1);
        int s1 = std::clamp(static_cast<int>(std::ceil(startSample + (px + 1) * samplesPerPixel)), s0 + 1, totalSamples);

        float maxV = -1.0f, minV = 1.0f;
        for (int s = s0; s < s1; ++s)
        {
            float v = m_audioData.samples[s];
            maxV = std::max(maxV, v);
            minV = std::min(minV, v);
        }

        int halfH = rect.height() / 2;
        int topY = std::clamp(centerY - static_cast<int>(maxV * halfH * 0.9f), rect.top(), rect.bottom());
        int botY = std::clamp(centerY - static_cast<int>(minV * halfH * 0.9f), rect.top(), rect.bottom());

        if (first) { pathTop.moveTo(rect.left() + px, topY); pathBottom.moveTo(rect.left() + px, botY); first = false; }
        else { pathTop.lineTo(rect.left() + px, topY); pathBottom.lineTo(rect.left() + px, botY); }
    }

    // Fill between
    QPainterPath fillPath = pathTop;
    for (int i = pathBottom.elementCount() - 1; i >= 0; --i)
        fillPath.lineTo(QPointF(pathBottom.elementAt(i).x, pathBottom.elementAt(i).y));
    fillPath.closeSubpath();

    p.fillPath(fillPath, kWaveformFill);
    p.setPen(QPen(kWaveformLine, 1.2));
    p.drawPath(pathTop);
    p.drawPath(pathBottom);
}
