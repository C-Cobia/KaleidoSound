#pragma once

#include <QString>
#include <QUuid>
#include "../audio/AudioAnalyzer.h"
#include "../audio/BeatAnalysisResult.h"

struct AudioAsset
{
    QString id;         // QUuid
    QString name;       // Display name (basename)
    QString filePath;   // Full path
    AudioData data;     // Loaded samples
    BeatAnalysisResult beats;  // Beat detection results
    double duration = 0;
    double bpm = 0;
    int sampleRate = 44100;
    int channels = 1;
    bool analyzed = false;

    static AudioAsset create(const QString& filePath)
    {
        AudioAsset a;
        a.id = QUuid::createUuid().toString();
        a.filePath = filePath;
        a.name = filePath.section('/', -1).section('\\', -1); // basename
        return a;
    }
};
