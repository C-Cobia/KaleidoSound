#include "AssetManager.h"
#include "../audio/AudioLoadWorker.h"
#include "../audio/AnalysisManager.h"

AssetManager::AssetManager(QObject* parent)
    : QObject(parent)
{
}

AssetManager::~AssetManager()
{
    const auto threads = m_loadThreads;
    for (QThread* thread : threads)
    {
        if (thread && thread->isRunning())
        {
            thread->quit();
            thread->wait();
        }
    }
}

QString AssetManager::addAudio(const QString& filePath)
{
    // Deduplicate: check if this file path already exists
    for (auto it = m_audioAssets.constBegin(); it != m_audioAssets.constEnd(); ++it)
    {
        if (it.value().filePath == filePath)
            return it.key();
    }

    AudioAsset asset = AudioAsset::create(filePath);
    QString id = asset.id;
    m_audioAssets.insert(id, asset);

    emit audioAdded(id);

    auto* worker = new AudioLoadWorker(filePath, m_analysisManager);
    auto* thread = new QThread(this);
    m_loadThreads.append(thread);
    worker->moveToThread(thread);

    connect(worker, &AudioLoadWorker::progress, this, [this, id](int percent) {
        emit audioLoadProgress(id, percent);
    });
    connect(worker, &AudioLoadWorker::error, this, [this, id](const QString& error) {
        emit audioLoadError(id, error);
    });
    connect(worker, &AudioLoadWorker::finished, this, [this, id](const AudioData& data, const BeatAnalysisResult& beats) {
        auto it = m_audioAssets.find(id);
        if (it != m_audioAssets.end())
        {
            it.value().data = data;
            it.value().beats = beats;
            it.value().duration = data.durationSec;
            it.value().sampleRate = data.sampleRate;
            it.value().channels = data.channels;
            it.value().bpm = beats.bpm;
            it.value().analyzed = true;
        }

        emit audioLoaded(id);
    });
    connect(worker, &AudioLoadWorker::finished, thread, &QThread::quit);
    connect(worker, &AudioLoadWorker::error, thread, &QThread::quit);
    connect(thread, &QThread::finished, worker, &QObject::deleteLater);
    connect(thread, &QThread::finished, thread, &QObject::deleteLater);
    connect(thread, &QThread::finished, this, [this, thread]() {
        m_loadThreads.removeAll(thread);
    });

    connect(thread, &QThread::started, worker, &AudioLoadWorker::process);
    thread->start();

    return id;
}

bool AssetManager::hasAudio(const QString& assetId) const
{
    return m_audioAssets.contains(assetId);
}

AudioAsset* AssetManager::audioAsset(const QString& assetId)
{
    auto it = m_audioAssets.find(assetId);
    return (it != m_audioAssets.end()) ? &it.value() : nullptr;
}

QList<AudioAsset*> AssetManager::allAudioAssets() const
{
    QList<AudioAsset*> result;
    for (auto it = m_audioAssets.constBegin(); it != m_audioAssets.constEnd(); ++it)
        result.append(const_cast<AudioAsset*>(&it.value()));
    return result;
}

QString AssetManager::addSTL(const QString& filePath)
{
    // Deduplicate: check if this file path already exists
    for (auto it = m_stlAssets.constBegin(); it != m_stlAssets.constEnd(); ++it)
    {
        if (it.value().filePath == filePath)
            return it.key();
    }

    STLAsset asset = STLAsset::create(filePath);
    QString id = asset.id;
    m_stlAssets.insert(id, asset);
    emit stlAdded(id);
    return id;
}

bool AssetManager::hasSTL(const QString& assetId) const
{
    return m_stlAssets.contains(assetId);
}

STLAsset* AssetManager::stlAsset(const QString& assetId)
{
    auto it = m_stlAssets.find(assetId);
    return (it != m_stlAssets.end()) ? &it.value() : nullptr;
}

QList<STLAsset*> AssetManager::allSTLAssets() const
{
    QList<STLAsset*> result;
    for (auto it = m_stlAssets.constBegin(); it != m_stlAssets.constEnd(); ++it)
        result.append(const_cast<STLAsset*>(&it.value()));
    return result;
}

void AssetManager::removeAsset(const QString& assetId)
{
    if (m_audioAssets.remove(assetId) > 0 || m_stlAssets.remove(assetId) > 0)
        emit assetRemoved(assetId);
}
