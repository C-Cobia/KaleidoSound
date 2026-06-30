#include "BeatNetDetector.h"
#include "BeatDetector.h"

BeatNetDetector::BeatNetDetector()
{
    m_detector = new BeatDetector();
}

BeatNetDetector::~BeatNetDetector()
{
    delete m_detector;
}

bool BeatNetDetector::isAvailable() const
{
    return m_detector && m_detector->isModelLoaded();
}

bool BeatNetDetector::loadModel(const QString& modelPath)
{
    if (!m_detector) return false;
    return m_detector->loadModel(modelPath);
}

BeatAnalysisResult BeatNetDetector::analyze(const AudioData& audio)
{
    BeatAnalysisResult result;
    result.method = "beatnet";

    if (!isAvailable())
        return result;

    BeatInfo info = m_detector->detect(audio);

    result.bpm = info.bpm;
    result.activation = info.activation;
    result.downbeatActivation = info.downbeatActivation;
    result.activationSampleRate = info.activationSampleRate;

    for (int i = 0; i < info.candidatePeakTimes.size(); ++i)
    {
        BeatEvent event;
        event.timeSec = info.candidatePeakTimes[i];
        event.confidence = i < info.candidatePeakScores.size() ? info.candidatePeakScores[i] : 1.0f;
        result.candidateBeats.append(event);
    }

    for (int i = 0; i < info.beatTimes.size(); ++i)
    {
        BeatEvent event;
        event.timeSec = info.beatTimes[i];
        event.confidence = i < info.beatConfidences.size() ? info.beatConfidences[i] : 1.0f;
        event.isDownbeat = i < info.beatIsDownbeat.size() && info.beatIsDownbeat[i];
        result.beats.append(event);
    }

    // Pass tempo diagnostics
    result.intervalHistogram = info.intervalHistogram;
    result.histogramMinSec = info.histogramMinSec;
    result.histogramMaxSec = info.histogramMaxSec;
    result.selectedTempoSec = info.selectedTempoSec;
    result.selectedBpm = info.selectedBpm;
    result.tempoScore = info.tempoScore;
    result.candidateCount = info.candidateCount;
    result.medianCandidateInterval = info.medianCandidateInterval;
    result.attackPhaseOffsetSec = info.attackPhaseOffsetSec;
    result.perceptualShiftSec = info.perceptualShiftSec;
    result.attackPhaseMatches = info.attackPhaseMatches;

    if (info.beatTimes.isEmpty())
        return result;

    result.success = true;
    return result;
}

bool BeatNetDetector::dumpFeatures(const AudioData& data, const QString& outputPath)
{
    if (!m_detector) return false;
    return m_detector->dumpFeatures(data, outputPath);
}
