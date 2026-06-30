#pragma once

#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QLineEdit>
#include <QFrame>
#include <QMap>

class AssetManager;

class AssetBrowser : public QWidget
{
    Q_OBJECT

public:
    explicit AssetBrowser(AssetManager* assetManager, QWidget* parent = nullptr);

signals:
    void assetSelected(const QString& assetId);
    void importRequested();

private slots:
    void onAudioAdded(const QString& assetId);
    void onAudioLoadProgress(const QString& assetId, int percent);
    void onAudioLoaded(const QString& assetId);
    void onAudioLoadError(const QString& assetId, const QString& error);
    void onSTLAdded(const QString& assetId);
    void onAssetRemoved(const QString& assetId);
    void onItemClicked(QListWidgetItem* item);

private:
    void updateCountLabel();

    AssetManager* m_assetManager = nullptr;
    QListWidget* m_listWidget = nullptr;
    QLabel* m_countLabel = nullptr;
    QMap<QString, QListWidgetItem*> m_itemMap;
};
