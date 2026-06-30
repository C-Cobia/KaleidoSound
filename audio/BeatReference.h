#pragma once

#include <QString>
#include "BeatAnalysisResult.h"

namespace BeatReference
{
    QVector<BeatEvent> loadFromFile(const QString& path);
    QVector<BeatEvent> loadSidecarForAudio(const QString& audioPath, QString* loadedPath = nullptr);
}
