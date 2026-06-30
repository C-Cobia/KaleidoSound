#pragma once

#include <QObject>
#include <QString>
#include "AudioAnalyzer.h"
#include "BeatAnalysisResult.h"

class AudioLoadWorker : public QObject
{
    Q_OBJECT

public:
    explicit AudioLoadWorker(const QString& filePath, AnalysisManager* analysisManager, QObject* parent = nullptr);

public slots:
    void process();

signals:
    void progress(int percent);
    void finished(const AudioData& data, const BeatAnalysisResult& beats);
    void error(const QString& message);

private:
    QString m_filePath;
    AnalysisManager* m_analysisManager = nullptr;
};
