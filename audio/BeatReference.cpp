#include "BeatReference.h"

#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QTextStream>
#include <algorithm>

namespace {

void appendBeat(QVector<BeatEvent>& beats, double timeSec, bool downbeat = false)
{
    if (timeSec < 0.0 || !std::isfinite(timeSec))
        return;

    BeatEvent event;
    event.timeSec = timeSec;
    event.confidence = 1.0f;
    event.isDownbeat = downbeat;
    beats.append(event);
}

QVector<BeatEvent> loadJson(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return {};

    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    QJsonArray array;
    if (doc.isArray())
    {
        array = doc.array();
    }
    else if (doc.isObject())
    {
        const QJsonObject obj = doc.object();
        if (obj.value("beats").isArray())
            array = obj.value("beats").toArray();
        else if (obj.value("reference_beats").isArray())
            array = obj.value("reference_beats").toArray();
    }

    QVector<BeatEvent> beats;
    beats.reserve(array.size());
    for (const QJsonValue& value : array)
    {
        if (value.isDouble())
        {
            appendBeat(beats, value.toDouble());
        }
        else if (value.isObject())
        {
            const QJsonObject obj = value.toObject();
            const double timeSec = obj.contains("time") ? obj.value("time").toDouble()
                                 : obj.contains("timeSec") ? obj.value("timeSec").toDouble()
                                 : obj.contains("seconds") ? obj.value("seconds").toDouble()
                                 : -1.0;
            appendBeat(beats, timeSec, obj.value("downbeat").toBool(false));
        }
    }
    return beats;
}

QVector<BeatEvent> loadCsv(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return {};

    QVector<BeatEvent> beats;
    QTextStream in(&file);
    while (!in.atEnd())
    {
        const QString line = in.readLine().trimmed();
        if (line.isEmpty() || line.startsWith('#'))
            continue;

        const QStringList parts = line.split(QRegularExpression("[,;\\t]"), Qt::SkipEmptyParts);
        if (parts.isEmpty())
            continue;

        bool ok = false;
        double timeSec = parts[0].trimmed().toDouble(&ok);
        if (!ok)
            continue;

        bool downbeat = false;
        if (parts.size() > 1)
        {
            const QString marker = parts[1].trimmed().toLower();
            downbeat = marker == "1" || marker == "true" || marker == "downbeat";
        }
        appendBeat(beats, timeSec, downbeat);
    }
    return beats;
}

} // namespace

QVector<BeatEvent> BeatReference::loadFromFile(const QString& path)
{
    QVector<BeatEvent> beats;
    const QString lower = QFileInfo(path).suffix().toLower();
    if (lower == "json")
        beats = loadJson(path);
    else
        beats = loadCsv(path);

    std::sort(beats.begin(), beats.end());
    return beats;
}

QVector<BeatEvent> BeatReference::loadSidecarForAudio(const QString& audioPath, QString* loadedPath)
{
    const QFileInfo info(audioPath);
    const QString base = info.absolutePath() + "/" + info.completeBaseName();
    const QStringList candidates = {
        base + ".beats.csv",
        base + ".beatgrid.csv",
        base + ".beats.json",
        base + ".beatgrid.json",
    };

    for (const QString& path : candidates)
    {
        if (!QFile::exists(path))
            continue;

        QVector<BeatEvent> beats = loadFromFile(path);
        if (!beats.isEmpty())
        {
            if (loadedPath)
                *loadedPath = path;
            return beats;
        }
    }

    return {};
}
