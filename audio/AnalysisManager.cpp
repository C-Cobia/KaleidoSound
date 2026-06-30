#include "AnalysisManager.h"
#include "IBeatDetector.h"
#include "BeatNetDetector.h"

#include <QThread>
#include <algorithm>

AnalysisManager::AnalysisManager(QObject* parent)
    : QObject(parent)
{
}

AnalysisManager::~AnalysisManager()
{
    if (m_workerThread && m_workerThread->isRunning())
    {
        m_workerThread->quit();
        m_workerThread->wait(3000);
    }

    qDeleteAll(m_detectors);
    m_detectors.clear();
}

void AnalysisManager::registerDetector(IBeatDetector* detector)
{
    if (detector)
        m_detectors.append(detector);
}

IBeatDetector* AnalysisManager::findDetector(const QString& name) const
{
    for (auto* d : m_detectors)
    {
        if (QString(d->name()) == name && d->isAvailable())
            return d;
    }
    return nullptr;
}

QString AnalysisManager::defaultDetector() const
{
    // Offline imports should prefer BeatNet when it is available; fall back to
    // lighter realtime detectors when the model is not present or failed.
    if (auto* beatnet = findDetector("beatnet"))
        return beatnet->name();

    for (auto* d : m_detectors)
    {
        if (d->isAvailable() && d->isRealtime())
            return d->name();
    }
    // Fallback to first available
    for (auto* d : m_detectors)
    {
        if (d->isAvailable())
            return d->name();
    }
    return "";
}

QStringList AnalysisManager::availableDetectors() const
{
    QStringList list;
    for (auto* d : m_detectors)
    {
        if (d->isAvailable())
            list.append(d->name());
    }
    return list;
}

BeatAnalysisResult AnalysisManager::analyzeSync(const AudioData& audio, const QString& preferredMethod)
{
    IBeatDetector* detector = nullptr;

    if (!preferredMethod.isEmpty())
        detector = findDetector(preferredMethod);

    if (!detector)
        detector = findDetector(defaultDetector());

    if (!detector)
    {
        BeatAnalysisResult r;
        r.method = "none";
        return r;
    }

    BeatAnalysisResult result = detector->analyze(audio);
    qInfo("AnalysisManager: detector=%s success=%d beats=%d bpm=%.2f",
          detector->name(), result.success ? 1 : 0, result.beatCount(), result.bpm);

    if (!result.success && QString(detector->name()) != "aubio")
    {
        if (auto* fallback = findDetector("aubio"))
        {
            BeatAnalysisResult debugResult = result;
            result = fallback->analyze(audio);
            if (result.activation.isEmpty() && !debugResult.activation.isEmpty())
            {
                result.activation = debugResult.activation;
                result.downbeatActivation = debugResult.downbeatActivation;
                result.activationSampleRate = debugResult.activationSampleRate;
                result.activationOffset = debugResult.activationOffset;
                result.candidateBeats = debugResult.candidateBeats;
                result.method += "+beatnet-debug";
            }
            qInfo("AnalysisManager: fallback detector=%s success=%d beats=%d bpm=%.2f",
                  fallback->name(), result.success ? 1 : 0, result.beatCount(), result.bpm);
        }
    }

    return result;
}

void AnalysisManager::analyzeAsync(const QString& assetId, const AudioData& audio, const QString& preferredMethod)
{
    if (m_isAnalyzing)
    {
        qWarning("AnalysisManager: analysis already in progress");
        return;
    }

    IBeatDetector* detector = nullptr;
    if (!preferredMethod.isEmpty())
        detector = findDetector(preferredMethod);
    if (!detector)
        detector = findDetector(defaultDetector());

    if (!detector)
    {
        BeatAnalysisResult r;
        r.method = "none";
        emit analysisError(assetId, "No detector available");
        return;
    }

    m_isAnalyzing = true;
    m_currentAssetId = assetId;
    emit analysisStarted(assetId);

    // Run in worker thread
    auto* audioCopy = new AudioData(audio);
    auto* det = detector;

    m_workerThread = QThread::create([this, assetId, audioCopy, det]() {
        BeatAnalysisResult result = det->analyze(*audioCopy);
        if (!result.success && QString(det->name()) != "aubio")
        {
            if (auto* fallback = findDetector("aubio"))
            {
                BeatAnalysisResult debugResult = result;
                result = fallback->analyze(*audioCopy);
                if (result.activation.isEmpty() && !debugResult.activation.isEmpty())
                {
                    result.activation = debugResult.activation;
                    result.downbeatActivation = debugResult.downbeatActivation;
                    result.activationSampleRate = debugResult.activationSampleRate;
                    result.activationOffset = debugResult.activationOffset;
                    result.candidateBeats = debugResult.candidateBeats;
                    result.method += "+beatnet-debug";
                }
            }
        }
        delete audioCopy;

        // Emit result on main thread
        QMetaObject::invokeMethod(this, [this, assetId, result]() {
            m_cache.insert(assetId, result);
            m_isAnalyzing = false;
            emit analysisFinished(assetId, result);
        }, Qt::QueuedConnection);
    });

    connect(m_workerThread, &QThread::finished, m_workerThread, &QObject::deleteLater);
    m_workerThread->start();
}

BeatAnalysisResult AnalysisManager::cachedResult(const QString& assetId) const
{
    return m_cache.value(assetId);
}

bool AnalysisManager::dumpFeatures(const AudioData& data, const QString& outputPath)
{
    for (auto* d : m_detectors)
    {
        if (QString(d->name()) == "beatnet" && d->isAvailable())
        {
            // BeatNetDetector has dumpFeatures — call through static cast
            // (we know it's a BeatNetDetector because name() == "beatnet")
            auto* beatnet = static_cast<BeatNetDetector*>(d);
            return beatnet->dumpFeatures(data, outputPath);
        }
    }
    qWarning("AnalysisManager: No BeatNet detector available for feature dump");
    return false;
}
