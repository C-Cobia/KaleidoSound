#include "TransportController.h"
#include "AudioAnalyzer.h"
#include <QAudioDevice>
#include <algorithm>

TransportController::TransportController(QObject* parent)
    : QObject(parent)
{
    m_player = new QMediaPlayer(this);
    m_audioOutput = new QAudioOutput(QMediaDevices::defaultAudioOutput(), this);
    m_player->setAudioOutput(m_audioOutput);

    connect(m_player, &QMediaPlayer::positionChanged, this, &TransportController::onPlayerPositionChanged);
    connect(m_player, &QMediaPlayer::durationChanged, this, &TransportController::onPlayerDurationChanged);

    // Handle audio device hot-swap
    connect(&m_mediaDevices, &QMediaDevices::audioOutputsChanged, this, [this]() {
        QAudioDevice device = QMediaDevices::defaultAudioOutput();
        if (device.isNull()) return;

        bool wasPlaying = m_isPlaying;
        qint64 position = m_player->position();

        m_player->stop();
        delete m_audioOutput;
        m_audioOutput = new QAudioOutput(device, this);
        m_player->setAudioOutput(m_audioOutput);

        if (wasPlaying && m_hasSource)
        {
            m_player->setPosition(position);
            m_player->play();
        }
    });
}

TransportController::~TransportController() = default;

void TransportController::loadSource(const QString& filePath)
{
    m_player->stop();
    m_isPlaying = false;
    m_currentTimeSec = 0.0;
    m_lastSpectrumMs = -1;

    m_player->setSource(QUrl::fromLocalFile(filePath));
    m_hasSource = true;

    emit positionChanged(0.0);
    emit sourceLoaded(filePath);
}

double TransportController::durationSec() const
{
    return m_durationSec;
}

void TransportController::play()
{
    if (!m_hasSource) return;
    m_player->play();
    m_isPlaying = true;
    emit playbackStarted();
}

void TransportController::pause()
{
    if (!m_hasSource) return;
    m_player->pause();
    m_isPlaying = false;
    emit playbackPaused();
}

void TransportController::stop()
{
    if (!m_hasSource) return;
    m_player->stop();
    m_isPlaying = false;
    m_currentTimeSec = 0.0;
    m_lastSpectrumMs = -1;
    emit playbackStopped();
    emit positionChanged(0.0);
}

void TransportController::seek(double sec)
{
    if (!m_hasSource) return;
    sec = std::clamp(sec, 0.0, m_durationSec);
    m_player->setPosition(static_cast<qint64>(sec * 1000));
    m_currentTimeSec = sec;
    m_lastSpectrumMs = -1;  // Force spectrum recomputation on seek
    emit positionChanged(sec);
}

void TransportController::onPlayerPositionChanged(qint64 ms)
{
    m_currentTimeSec = static_cast<double>(ms) / 1000.0;

    // Compute spectrum at current position (throttled to 80ms)
    if (m_analyzer && m_hasSource &&
        (m_lastSpectrumMs < 0 || std::abs(ms - m_lastSpectrumMs) >= 80))
    {
        AudioData data = m_analyzer->audioData();
        if (data.sampleRate > 0)
        {
            int startSample = static_cast<int>(m_currentTimeSec * data.sampleRate);
            m_currentSpectrum = m_analyzer->computeSpectrum(data, startSample, 1024);
            m_lastSpectrumMs = ms;
        }
    }

    emit positionChanged(m_currentTimeSec);
}

void TransportController::onPlayerDurationChanged(qint64 ms)
{
    m_durationSec = static_cast<double>(ms) / 1000.0;
    emit durationChanged(m_durationSec);
}
