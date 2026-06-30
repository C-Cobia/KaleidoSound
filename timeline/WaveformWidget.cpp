#include "WaveformWidget.h"
#include "../audio/TransportController.h"
#include "TimelineViewState.h"
#include "../audio/AudioLoadWorker.h"

#include <QPainter>
#include <QPainterPath>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QUrl>
#include <QThread>
#include <algorithm>
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace {

constexpr auto kBgDark        = QColor(18, 18, 24);
constexpr auto kBgPanel       = QColor(24, 24, 32);
constexpr auto kBgWaveform    = QColor(14, 14, 20);
constexpr auto kWaveformFill  = QColor(0, 180, 220, 60);
constexpr auto kWaveformLine  = QColor(0, 200, 240);
constexpr auto kAccentColor   = QColor(255, 80, 120);
constexpr auto kBeatColor     = QColor(255, 200, 60, 140);
constexpr auto kPlayheadColor = QColor(255, 255, 255, 200);
constexpr auto kTextColor     = QColor(160, 160, 180);
constexpr auto kTextDim       = QColor(100, 100, 120);
constexpr auto kGridColor     = QColor(40, 40, 55);
constexpr auto kSpectrumGrad0 = QColor(0, 60, 120);
constexpr auto kSpectrumGrad2 = QColor(255, 100, 50);
constexpr auto kBorderLine    = QColor(50, 50, 70);

} // namespace

WaveformWidget::WaveformWidget(QWidget* parent)
    : QWidget(parent)
{
    setMinimumHeight(220);
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

WaveformWidget::~WaveformWidget()
{
    if (m_loadThread && m_loadThread->isRunning())
    {
        m_loadThread->quit();
        m_loadThread->wait();
    }
}

// ============================================================================
// Viewport helpers (delegate to TimelineViewState)
// ============================================================================

static inline double viewStart(TimelineViewState* s) { return s ? s->viewStart() : 0.0; }
static inline double viewDuration(TimelineViewState* s) { return s ? s->viewDuration() : 10.0; }
static inline double viewEnd(TimelineViewState* s) { return s ? s->viewEnd() : 10.0; }

// ============================================================================
// Audio Loading
// ============================================================================

void WaveformWidget::loadAudio(const QString& filePath)
{
    if (m_loadThread && m_loadThread->isRunning())
    {
        qWarning("WaveformWidget: audio load already in progress");
        return;
    }

    m_isLoading = true;
    m_loadingProgress = 0;
    m_pendingFilePath = filePath;

    auto* worker = new AudioLoadWorker(filePath, nullptr);
    auto* thread = new QThread(this);
    m_loadThread = thread;
    worker->moveToThread(thread);

    connect(worker, &AudioLoadWorker::progress, this, &WaveformWidget::onLoadProgress);
    connect(worker, &AudioLoadWorker::error, this, [this](const QString& err) {
        qWarning("WaveformWidget: %s", qPrintable(err));
        m_isLoading = false;
        m_loadingProgress = 0;
        update();
    });
    connect(worker, &AudioLoadWorker::finished, this, &WaveformWidget::onLoadFinished);
    connect(worker, &AudioLoadWorker::finished, thread, &QThread::quit);
    connect(worker, &AudioLoadWorker::error, thread, &QThread::quit);
    connect(thread, &QThread::finished, worker, &QObject::deleteLater);
    connect(thread, &QThread::finished, thread, &QObject::deleteLater);
    connect(thread, &QThread::finished, this, [this, thread]() {
        if (m_loadThread == thread) m_loadThread = nullptr;
    });
    connect(thread, &QThread::started, worker, &AudioLoadWorker::process);
    thread->start();
    update();
}

void WaveformWidget::setAudioData(const AudioData& data, const BeatAnalysisResult& beats, const QString& filePath)
{
    m_audioData = data;
    m_beats = beats;
    m_hasAudio = true;
    m_isLoading = false;
    m_loadingProgress = 100;
    m_pendingFilePath = filePath;

    computeWaveformDownsample();

    if (m_transport)
        m_transport->loadSource(filePath);

    if (m_timelineState)
        m_timelineState->setTotalDuration(data.durationSec);

    m_currentPositionSec = 0.0;
    m_lastBeatIndex = -1;
    update();
}

void WaveformWidget::onLoadProgress(int percent)
{
    m_loadingProgress = percent;
    update();
}

void WaveformWidget::onLoadFinished(const AudioData& data, const BeatAnalysisResult& beats)
{
    m_audioData = data;
    m_beats = beats;
    m_hasAudio = true;
    m_isLoading = false;
    m_loadingProgress = 100;

    computeWaveformDownsample();

    if (m_transport)
        m_transport->loadSource(m_pendingFilePath);

    if (m_timelineState)
        m_timelineState->setTotalDuration(data.durationSec);

    m_currentPositionSec = 0.0;
    m_lastBeatIndex = -1;
    update();
}

void WaveformWidget::clearAudio()
{
    if (m_transport) m_transport->stop();
    m_hasAudio = false;
    m_lastBeatIndex = -1;
    m_waveformMax.clear();
    m_waveformMin.clear();
    m_beats = BeatAnalysisResult{};
    m_currentSpectrum = SpectrumFrame{};
    update();
}

// ============================================================================
// Transport callbacks
// ============================================================================

void WaveformWidget::setMetronomeEnabled(bool enabled)
{
    m_metronomeOn = enabled;
    if (enabled && !m_beats.beats.isEmpty())
    {
        const auto it = std::upper_bound(m_beats.beats.cbegin(), m_beats.beats.cend(), m_currentPositionSec,
            [](double t, const BeatEvent& e) { return t < e.timeSec; });
        m_lastBeatIndex = static_cast<int>(std::distance(m_beats.beats.cbegin(), it)) - 1;
    }
    else
    {
        m_lastBeatIndex = -1;
    }
    update();
}

void WaveformWidget::playClickSound()
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

void WaveformWidget::onPositionChanged(double sec)
{
    if (!m_hasAudio) return;

    m_currentPositionSec = sec;

    // Soft follow via TimelineViewState
    if (m_timelineState)
        m_timelineState->updateFollow(sec);

    // Spectrum from transport
    if (m_transport)
        m_currentSpectrum = m_transport->currentSpectrum();

    // Metronome
    updateBeatMetronome(sec);

    update();
}

void WaveformWidget::updateBeatMetronome(double timeSec)
{
    if (!m_metronomeOn || m_beats.beats.isEmpty()) return;

    const auto it = std::upper_bound(m_beats.beats.cbegin(), m_beats.beats.cend(), timeSec,
        [](double t, const BeatEvent& e) { return t < e.timeSec; });
    const int idx = static_cast<int>(std::distance(m_beats.beats.cbegin(), it)) - 1;

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

void WaveformWidget::computeWaveformDownsample()
{
    const int displayWidth = std::max(width(), 1200);
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

// ============================================================================
// Paint
// ============================================================================

void WaveformWidget::paintEvent(QPaintEvent* /*event*/)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    const QRect fullRect = rect();
    p.fillRect(fullRect, kBgDark);

    const double vs = viewStart(m_timelineState);
    const double vd = viewDuration(m_timelineState);
    const double ve = viewEnd(m_timelineState);

    if (!m_hasAudio)
    {
        p.setPen(kTextDim);
        p.setFont(QFont("Segoe UI", 13));
        p.drawText(fullRect, Qt::AlignCenter, "Import audio to begin analysis");
        if (m_isLoading) drawProgressBar(p, fullRect);
        return;
    }

    const int toolbarH = 28, spectrumH = 100, rulerH = 22, margin = 8;
    const int waveH = fullRect.height() - toolbarH - spectrumH - rulerH - margin * 3;

    // Toolbar
    QRect toolbarRect(margin, margin, fullRect.width() - margin * 2, toolbarH);
    p.fillRect(toolbarRect, kBgPanel);
    p.setPen(kBorderLine);
    p.drawRect(toolbarRect);

    if (m_beats.bpm > 0)
    {
        p.setPen(kAccentColor);
        p.setFont(QFont("Consolas", 10, QFont::Bold));
        p.drawText(toolbarRect.adjusted(8, 0, 0, 0), Qt::AlignVCenter | Qt::AlignLeft,
            QStringLiteral("BPM: %1").arg(m_beats.bpm, 0, 'f', 1));
    }

    p.setPen(kTextColor);
    p.setFont(QFont("Segoe UI", 9));
    p.drawText(toolbarRect.adjusted(130, 0, 0, 0), Qt::AlignVCenter | Qt::AlignLeft,
        QStringLiteral("%1 beats").arg(m_beats.beats.size()));

    // Metronome button
    {
        int btnW = 70, btnH = 20;
        int btnX = toolbarRect.left() + 280;
        int btnY = toolbarRect.top() + (toolbarRect.height() - btnH) / 2;
        QRect metrBtn(btnX, btnY, btnW, btnH);
        if (m_metronomeOn) { p.fillRect(metrBtn, QColor(0, 180, 220, 100)); p.setPen(QPen(QColor(0, 200, 240), 1)); }
        else { p.setPen(kTextDim); }
        p.setFont(QFont("Segoe UI", 8));
        p.drawRect(metrBtn);
        p.drawText(metrBtn, Qt::AlignCenter, "Metronome");
    }

    p.setPen(kTextColor);
    p.drawText(toolbarRect.adjusted(0, 0, -8, 0), Qt::AlignVCenter | Qt::AlignRight,
        QStringLiteral("Duration: %1s").arg(m_audioData.durationSec, 0, 'f', 1));

    p.setPen(kTextDim);
    p.drawText(toolbarRect.adjusted(0, 0, -180, 0), Qt::AlignVCenter | Qt::AlignRight,
        m_viewMode == ViewMode::Waveform ? "Waveform" : "Spectrum");

    // Waveform
    QRect waveRect(margin, toolbarH + margin * 2, fullRect.width() - margin * 2, std::max(waveH, 60));
    drawWaveform(p, waveRect, vs, vd);

    // Spectrum
    QRect specRect(margin, waveRect.bottom() + margin, waveRect.width(), spectrumH);
    drawSpectrum(p, specRect);

    // Time ruler
    QRect rulerRect(margin, specRect.bottom() + margin, waveRect.width(), rulerH);
    drawTimeRuler(p, rulerRect, vs, vd);

    if (m_isLoading) drawProgressBar(p, fullRect);
}

void WaveformWidget::drawWaveform(QPainter& p, const QRect& rect, double vs, double vd)
{
    p.fillRect(rect, kBgWaveform);
    p.setPen(kBorderLine);
    p.drawRect(rect);

    int centerY = rect.center().y();
    p.setPen(QPen(kGridColor, 1, Qt::DashLine));
    p.drawLine(rect.left(), centerY, rect.right(), centerY);

    if (m_audioData.samples.isEmpty()) return;

    const int w = rect.width();
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
        for (int s = s0; s < s1; ++s) { float v = m_audioData.samples[s]; maxV = std::max(maxV, v); minV = std::min(minV, v); }

        int halfH = rect.height() / 2;
        int topY = std::clamp(centerY - static_cast<int>(maxV * halfH * 0.9f), rect.top(), rect.bottom());
        int botY = std::clamp(centerY - static_cast<int>(minV * halfH * 0.9f), rect.top(), rect.bottom());

        if (first) { pathTop.moveTo(rect.left() + px, topY); pathBottom.moveTo(rect.left() + px, botY); first = false; }
        else { pathTop.lineTo(rect.left() + px, topY); pathBottom.lineTo(rect.left() + px, botY); }
    }

    QPainterPath fillPath = pathTop;
    for (int i = pathBottom.elementCount() - 1; i >= 0; --i)
        fillPath.lineTo(QPointF(pathBottom.elementAt(i).x, pathBottom.elementAt(i).y));
    fillPath.closeSubpath();

    p.fillPath(fillPath, kWaveformFill);
    p.setPen(QPen(kWaveformLine, 1.2));
    p.drawPath(pathTop);
    p.drawPath(pathBottom);

    drawBeatMarkers(p, rect, vs, vd);
    drawPlayhead(p, rect, vs, vd);
}

void WaveformWidget::drawSpectrum(QPainter& p, const QRect& rect)
{
    p.fillRect(rect, kBgWaveform);
    p.setPen(kBorderLine);
    p.drawRect(rect);

    p.setPen(kTextDim);
    p.setFont(QFont("Segoe UI", 8));
    p.drawText(rect.adjusted(4, 2, 0, 0), "SPECTRUM");

    if (m_currentSpectrum.magnitudes.isEmpty()) return;

    const int w = rect.width(), h = rect.height() - 16, y0 = rect.top() + 14;
    const int numBins = m_currentSpectrum.magnitudes.size();

    float maxMag = 0.001f;
    for (float m : m_currentSpectrum.magnitudes) maxMag = std::max(maxMag, m);

    const int numBars = std::min(w / 3, 128);
    for (int bar = 0; bar < numBars; ++bar)
    {
        float frac = static_cast<float>(bar) / numBars;
        int binIdx = std::min(static_cast<int>(std::pow(frac, 2.0f) * (numBins - 1)), numBins - 1);
        float mag = std::min(1.0f, m_currentSpectrum.magnitudes[binIdx] / maxMag);
        float logMag = std::log10(1.0f + mag * 9.0f);

        int barH = static_cast<int>(logMag * h);
        int barX = rect.left() + bar * (w / numBars);
        int barW = std::max(1, w / numBars - 1);

        QColor barColor;
        if (logMag < 0.33f) barColor = kSpectrumGrad0;
        else if (logMag < 0.66f) barColor = QColor::fromHsv(180 + static_cast<int>(logMag * 120), 220, 240);
        else barColor = QColor::fromHsv(30 + static_cast<int>((logMag - 0.66f) * 300), 240, 255);
        barColor.setAlpha(180);
        p.fillRect(barX, y0 + h - barH, barW, barH, barColor);
    }
}

void WaveformWidget::drawBeatMarkers(QPainter& p, const QRect& rect, double vs, double vd)
{
    const int w = rect.width();
    const double ve = vs + vd;
    p.setPen(QPen(kBeatColor, 1.5, Qt::DashLine));

    for (const auto& beat : m_beats.beats)
    {
        double bt = beat.timeSec;
        if (bt < vs || bt > ve) continue;
        int x = rect.left() + static_cast<int>(((bt - vs) / vd) * w);
        p.drawLine(x, rect.top(), x, rect.bottom());
    }
}

void WaveformWidget::drawPlayhead(QPainter& p, const QRect& rect, double vs, double vd)
{
    const int w = rect.width();
    if (m_currentPositionSec < vs || m_currentPositionSec > vs + vd) return;

    int x = rect.left() + static_cast<int>(((m_currentPositionSec - vs) / vd) * w);

    p.setPen(QPen(kPlayheadColor, 2));
    p.drawLine(x, rect.top(), x, rect.bottom());

    QPolygonF tri;
    tri << QPointF(x - 5, rect.top()) << QPointF(x + 5, rect.top()) << QPointF(x, rect.top() + 7);
    p.setBrush(kPlayheadColor);
    p.setPen(Qt::NoPen);
    p.drawPolygon(tri);
}

void WaveformWidget::drawTimeRuler(QPainter& p, const QRect& rect, double vs, double vd)
{
    p.fillRect(rect, kBgPanel);
    p.setPen(kBorderLine);
    p.drawRect(rect);

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

void WaveformWidget::drawProgressBar(QPainter& p, const QRect& rect)
{
    const int barW = 160, barH = 6, margin = 12;
    int barX = rect.right() - barW - margin;
    int barY = rect.bottom() - barH - margin;

    p.setPen(Qt::NoPen);
    p.setBrush(QColor(40, 40, 55));
    p.drawRoundedRect(QRect(barX, barY, barW, barH), 3, 3);

    int fillW = static_cast<int>(barW * m_loadingProgress / 100.0);
    if (fillW > 0)
    {
        QLinearGradient grad(barX, barY, barX + fillW, barY);
        grad.setColorAt(0.0, QColor(0, 180, 220));
        grad.setColorAt(1.0, QColor(0, 230, 255));
        p.setBrush(grad);
        p.drawRoundedRect(QRect(barX, barY, fillW, barH), 3, 3);
    }

    p.setPen(kTextColor);
    p.setFont(QFont("Segoe UI", 7));
    p.drawText(barX, barY - 14, barW, 14, Qt::AlignCenter,
        QStringLiteral("Loading... %1%").arg(m_loadingProgress));
}

// ============================================================================
// Input
// ============================================================================

void WaveformWidget::mousePressEvent(QMouseEvent* event)
{
    if (!m_hasAudio || !m_transport || !m_timelineState) return;

    const int toolbarH = 28, margin = 8;
    const int spectrumH = 100, rulerH = 22;
    const int waveH = height() - toolbarH - spectrumH - rulerH - margin * 3;
    QRect waveRect(margin, toolbarH + margin * 2, width() - margin * 2, std::max(waveH, 60));

    if (waveRect.contains(event->pos()))
    {
        double frac = static_cast<double>(event->pos().x() - waveRect.left()) / waveRect.width();
        double seekTime = m_timelineState->viewStart() + frac * m_timelineState->viewDuration();

        bool wasPlaying = m_transport->isPlaying();
        m_transport->seek(seekTime);
        if (wasPlaying) m_transport->play();
    }

    if (event->pos().y() < toolbarH + margin)
    {
        int btnW = 70, btnH = 20, btnX = margin + 280;
        int btnY = margin + (toolbarH - btnH) / 2;
        QRect metrBtn(btnX, btnY, btnW, btnH);
        if (metrBtn.contains(event->pos())) { setMetronomeEnabled(!m_metronomeOn); return; }
        m_viewMode = (m_viewMode == ViewMode::Waveform) ? ViewMode::Spectrum : ViewMode::Waveform;
        update();
    }
}

void WaveformWidget::mouseMoveEvent(QMouseEvent* event) { QWidget::mouseMoveEvent(event); }

void WaveformWidget::wheelEvent(QWheelEvent* event)
{
    if (!m_hasAudio || !m_timelineState) return;

    QPoint numDegrees = event->angleDelta() / 120;
    if (numDegrees.isNull()) return;

    double zoomFactor = (numDegrees.y() > 0) ? 0.8 : 1.25;
    double cursorFrac = std::clamp(static_cast<double>(event->position().x() - 8) / (width() - 16), 0.0, 1.0);

    m_timelineState->zoom(zoomFactor, cursorFrac);
    update();
    event->accept();
}
