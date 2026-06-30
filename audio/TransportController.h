#pragma once

#include <QObject>
#include <QMediaPlayer>
#include <QAudioOutput>
#include <QMediaDevices>
#include "AudioAnalyzer.h"

class AudioAnalyzer;

class TransportController : public QObject
{
    Q_OBJECT

public:
    explicit TransportController(QObject* parent = nullptr);
    ~TransportController() override;

    // Source
    void loadSource(const QString& filePath);
    bool hasSource() const { return m_hasSource; }
    double durationSec() const;

    // Transport controls
    void play();
    void pause();
    void stop();
    void seek(double sec);
    bool isPlaying() const { return m_isPlaying; }

    // Current state (Single Source of Truth)
    double currentTimeSec() const { return m_currentTimeSec; }
    double totalDuration() const { return m_durationSec; }

    // Spectrum computation (subscribes to position)
    void setAnalyzer(AudioAnalyzer* analyzer) { m_analyzer = analyzer; }
    const SpectrumFrame& currentSpectrum() const { return m_currentSpectrum; }

signals:
    void positionChanged(double sec);
    void playbackStarted();
    void playbackPaused();
    void playbackStopped();
    void sourceLoaded(const QString& filePath);
    void durationChanged(double durationSec);

private slots:
    void onPlayerPositionChanged(qint64 ms);
    void onPlayerDurationChanged(qint64 ms);

private:
    QMediaPlayer* m_player = nullptr;
    QAudioOutput* m_audioOutput = nullptr;
    QMediaDevices m_mediaDevices;

    bool m_hasSource = false;
    bool m_isPlaying = false;
    double m_currentTimeSec = 0.0;
    double m_durationSec = 0.0;

    // Spectrum (owned here, computed on position change)
    AudioAnalyzer* m_analyzer = nullptr;
    SpectrumFrame m_currentSpectrum;
    qint64 m_lastSpectrumMs = -1;
};
