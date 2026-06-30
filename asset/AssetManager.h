#pragma once

#include <QObject>
#include <QMap>
#include <QList>
#include <QThread>
#include "../asset/AudioAsset.h"
#include "../asset/STLAsset.h"

class AnalysisManager;

class AssetManager : public QObject
{
    Q_OBJECT

public:
    explicit AssetManager(QObject* parent = nullptr);
    ~AssetManager() override;

    void setAnalysisManager(class AnalysisManager* manager) { m_analysisManager = manager; }

    // Audio
    QString addAudio(const QString& filePath);
    bool hasAudio(const QString& assetId) const;
    AudioAsset* audioAsset(const QString& assetId);
    QList<AudioAsset*> allAudioAssets() const;

    // STL
    QString addSTL(const QString& filePath);
    bool hasSTL(const QString& assetId) const;
    STLAsset* stlAsset(const QString& assetId);
    QList<STLAsset*> allSTLAssets() const;

    // Removal
    void removeAsset(const QString& assetId);

signals:
    void audioAdded(const QString& assetId);
    void audioLoadProgress(const QString& assetId, int percent);
    void audioLoaded(const QString& assetId);
    void audioLoadError(const QString& assetId, const QString& error);
    void stlAdded(const QString& assetId);
    void assetRemoved(const QString& assetId);

private:
    QMap<QString, AudioAsset> m_audioAssets;
    QMap<QString, STLAsset> m_stlAssets;

    AnalysisManager* m_analysisManager = nullptr;
    QList<QThread*> m_loadThreads;
};
