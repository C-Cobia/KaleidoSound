#pragma once

#include <QWidget>
#include <QVector>
#include <QSoundEffect>
#include <QUrl>

#include "../audio/AudioAnalyzer.h"

class TransportController;
class TimelineViewState;

class WaveformWidget : public QWidget
{
    Q_OBJECT

public:
    explicit WaveformWidget(QWidget* parent = nullptr);
    ~WaveformWidget() override;

    void setTransport(TransportController* transport) { m_transport = transport; }
    void setTimelineState(TimelineViewState* state) { m_timelineState = state; }

    void loadAudio(const QString& filePath);
    void setAudioData(const AudioData& data, const BeatAnalysisResult& beats, const QString& filePath);
    void clearAudio();

    void setMetronomeEnabled(bool enabled);
    bool isMetronomeEnabled() const { return m_metronomeOn; }

    bool hasAudio() const { return m_hasAudio; }

    // Called by TransportController (via MainWindow) when position changes
    void onPositionChanged(double sec);

signals:
    void importRequested();

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;

private slots:
    void onLoadProgress(int percent);
    void onLoadFinished(const AudioData& data, const BeatAnalysisResult& beats);

private:
    void computeWaveformDownsample();
    void drawWaveform(QPainter& p, const QRect& rect, double vs, double vd);
    void drawSpectrum(QPainter& p, const QRect& rect);
    void drawBeatMarkers(QPainter& p, const QRect& rect, double vs, double vd);
    void drawPlayhead(QPainter& p, const QRect& rect, double vs, double vd);
    void drawTimeRuler(QPainter& p, const QRect& rect, double vs, double vd);
    void drawProgressBar(QPainter& p, const QRect& rect);
    void playClickSound();
    void updateBeatMetronome(double timeSec);

    // Transport (not owned)
    TransportController* m_transport = nullptr;
    TimelineViewState* m_timelineState = nullptr;

    // Audio data
    AudioAnalyzer m_analyzer;
    BeatAnalysisResult m_beats;
    AudioData m_audioData;
    bool m_hasAudio = false;

    // Waveform downsampled for display
    QVector<float> m_waveformMax;
    QVector<float> m_waveformMin;

    // Spectrum (from TransportController)
    SpectrumFrame m_currentSpectrum;

    // Metronome
    bool m_metronomeOn = false;
    int m_lastBeatIndex = -1;
    QVector<QSoundEffect*> m_clickSounds;
    int m_nextClickSound = 0;

    // Loading state
    bool m_isLoading = false;
    int m_loadingProgress = 0;
    QThread* m_loadThread = nullptr;
    QString m_pendingFilePath;

    // Current playback position (cached from TransportController)
    double m_currentPositionSec = 0.0;

    // UI
    enum class ViewMode { Waveform, Spectrum };
    ViewMode m_viewMode = ViewMode::Waveform;
};
