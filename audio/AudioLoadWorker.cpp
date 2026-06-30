#include "AudioLoadWorker.h"
#include "AnalysisManager.h"
#include "BeatReference.h"

AudioLoadWorker::AudioLoadWorker(const QString& filePath, AnalysisManager* analysisManager, QObject* parent)
    : QObject(parent)
    , m_filePath(filePath)
    , m_analysisManager(analysisManager)
{
}

void AudioLoadWorker::process()
{
    AudioAnalyzer analyzer;

    // Stage 1: Load audio file (0-40%)
    emit progress(10);
    QString error;
    if (!analyzer.loadFile(m_filePath, &error))
    {
        emit this->error(error);
        return;
    }
    emit progress(40);

    AudioData data = analyzer.audioData();

    // Stage 2: Detect beats via AnalysisManager (40-85%)
    BeatAnalysisResult beats;
    if (m_analysisManager)
        beats = m_analysisManager->analyzeSync(data, "beatnet");
    QString referencePath;
    beats.referenceBeats = BeatReference::loadSidecarForAudio(m_filePath, &referencePath);
    if (!beats.referenceBeats.isEmpty())
        qInfo("AudioLoadWorker: loaded reference beat grid from %s (%d beats)",
              qPrintable(referencePath), beats.referenceBeats.size());
    emit progress(85);

    // Stage 3: Ready (85-100%)
    emit progress(100);

    emit finished(data, beats);
}
