#pragma once

#include <QObject>
#include <QHash>
#include <QThread>
#include "BeatAnalysisResult.h"
#include "AudioAnalyzer.h"

class IBeatDetector;

class AnalysisManager : public QObject
{
    Q_OBJECT

public:
    explicit AnalysisManager(QObject* parent = nullptr);
    ~AnalysisManager() override;

    // Register detectors
    void registerDetector(IBeatDetector* detector);

    // Synchronous analysis (for small files / realtime)
    BeatAnalysisResult analyzeSync(const AudioData& audio, const QString& preferredMethod = "");

    // Asynchronous analysis (for large files / offline ML)
    void analyzeAsync(const QString& assetId, const AudioData& audio, const QString& preferredMethod = "");

    // Check if analysis is in progress
    bool isAnalyzing() const { return m_isAnalyzing; }

    // Get cached result
    BeatAnalysisResult cachedResult(const QString& assetId) const;

    // Available detectors
    QStringList availableDetectors() const;
    QString defaultDetector() const;

    // Feature dump for validation (finds BeatNet detector)
    bool dumpFeatures(const AudioData& data, const QString& outputPath);

signals:
    void analysisStarted(const QString& assetId);
    void analysisProgress(const QString& assetId, int percent);
    void analysisFinished(const QString& assetId, const BeatAnalysisResult& result);
    void analysisError(const QString& assetId, const QString& error);

private:
    IBeatDetector* findDetector(const QString& name) const;

    QVector<IBeatDetector*> m_detectors;
    QHash<QString, BeatAnalysisResult> m_cache;

    // Async state
    QThread* m_workerThread = nullptr;
    QString m_currentAssetId;
    bool m_isAnalyzing = false;
};
