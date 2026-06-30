#include "AssetBrowser.h"
#include "AssetManager.h"
#include "AudioAsset.h"
#include "STLAsset.h"

AssetBrowser::AssetBrowser(AssetManager* assetManager, QWidget* parent)
    : QWidget(parent)
    , m_assetManager(assetManager)
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    // Search/filter bar (Unity-style)
    auto* searchBar = new QWidget;
    searchBar->setObjectName("assetSearchBar");
    searchBar->setFixedHeight(28);
    auto* searchLayout = new QHBoxLayout(searchBar);
    searchLayout->setContentsMargins(8, 2, 8, 2);
    searchLayout->setSpacing(4);

    auto* filterLabel = new QLabel("All");
    filterLabel->setObjectName("assetFilterLabel");
    filterLabel->setFixedWidth(32);
    searchLayout->addWidget(filterLabel);

    auto* searchInput = new QLineEdit;
    searchInput->setObjectName("assetSearchInput");
    searchInput->setPlaceholderText("Search assets...");
    searchLayout->addWidget(searchInput);

    layout->addWidget(searchBar);

    // Separator
    auto* sep = new QFrame;
    sep->setFrameShape(QFrame::HLine);
    sep->setObjectName("assetSeparator");
    sep->setFixedHeight(1);
    layout->addWidget(sep);

    // Asset list
    m_listWidget = new QListWidget;
    m_listWidget->setObjectName("assetList");
    m_listWidget->setSpacing(1);
    m_listWidget->setUniformItemSizes(false);
    m_listWidget->setAlternatingRowColors(true);
    m_listWidget->setSelectionMode(QAbstractItemView::SingleSelection);
    connect(m_listWidget, &QListWidget::itemClicked, this, &AssetBrowser::onItemClicked);
    layout->addWidget(m_listWidget, 1);

    // Bottom status bar
    auto* statusBar = new QWidget;
    statusBar->setObjectName("assetStatusBar");
    statusBar->setFixedHeight(22);
    auto* statusLayout = new QHBoxLayout(statusBar);
    statusLayout->setContentsMargins(8, 0, 8, 0);
    statusLayout->setSpacing(4);

    m_countLabel = new QLabel("0 items");
    m_countLabel->setObjectName("assetCountLabel");
    statusLayout->addWidget(m_countLabel);
    statusLayout->addStretch();
    layout->addWidget(statusBar);

    // Connect AssetManager signals
    connect(m_assetManager, &AssetManager::audioAdded, this, &AssetBrowser::onAudioAdded);
    connect(m_assetManager, &AssetManager::audioLoadProgress, this, &AssetBrowser::onAudioLoadProgress);
    connect(m_assetManager, &AssetManager::audioLoaded, this, &AssetBrowser::onAudioLoaded);
    connect(m_assetManager, &AssetManager::audioLoadError, this, &AssetBrowser::onAudioLoadError);
    connect(m_assetManager, &AssetManager::stlAdded, this, &AssetBrowser::onSTLAdded);
    connect(m_assetManager, &AssetManager::assetRemoved, this, &AssetBrowser::onAssetRemoved);
}

void AssetBrowser::onAudioAdded(const QString& assetId)
{
    AudioAsset* asset = m_assetManager->audioAsset(assetId);
    if (!asset) return;

    auto* item = new QListWidgetItem;
    item->setText(asset->name);
    item->setData(Qt::UserRole, assetId);
    item->setData(Qt::UserRole + 1, "audio");
    item->setIcon(QIcon::fromTheme("audio-x-generic"));
    item->setToolTip("Loading...");
    m_listWidget->insertItem(0, item);
    m_itemMap.insert(assetId, item);
    m_listWidget->setCurrentItem(item);
    updateCountLabel();
}

void AssetBrowser::onAudioLoadProgress(const QString& assetId, int percent)
{
    auto it = m_itemMap.find(assetId);
    if (it != m_itemMap.end())
        (*it)->setToolTip(QStringLiteral("Loading... %1%").arg(percent));
}

void AssetBrowser::onAudioLoaded(const QString& assetId)
{
    AudioAsset* asset = m_assetManager->audioAsset(assetId);
    if (!asset) return;

    auto it = m_itemMap.find(assetId);
    if (it != m_itemMap.end())
    {
        (*it)->setToolTip(QStringLiteral("%1\nDuration: %2s\nBPM: %3\nSample Rate: %4 Hz\nChannels: %5")
            .arg(asset->name)
            .arg(asset->duration, 0, 'f', 2)
            .arg(asset->bpm, 0, 'f', 1)
            .arg(asset->sampleRate)
            .arg(asset->channels));
    }
}

void AssetBrowser::onAudioLoadError(const QString& assetId, const QString& error)
{
    auto it = m_itemMap.find(assetId);
    if (it != m_itemMap.end())
        (*it)->setToolTip(QStringLiteral("Load failed: %1").arg(error));
}

void AssetBrowser::onSTLAdded(const QString& assetId)
{
    STLAsset* asset = m_assetManager->stlAsset(assetId);
    if (!asset) return;

    auto* item = new QListWidgetItem;
    item->setText(asset->name);
    item->setData(Qt::UserRole, assetId);
    item->setData(Qt::UserRole + 1, "stl");
    item->setIcon(QIcon::fromTheme("3d"));
    item->setToolTip(asset->filePath);
    m_listWidget->insertItem(0, item);
    m_itemMap.insert(assetId, item);
    m_listWidget->setCurrentItem(item);
    updateCountLabel();
}

void AssetBrowser::onAssetRemoved(const QString& assetId)
{
    auto it = m_itemMap.find(assetId);
    if (it != m_itemMap.end())
    {
        delete *it;
        m_itemMap.erase(it);
        updateCountLabel();
    }
}

void AssetBrowser::onItemClicked(QListWidgetItem* item)
{
    if (item)
        emit assetSelected(item->data(Qt::UserRole).toString());
}

void AssetBrowser::updateCountLabel()
{
    if (!m_countLabel)
        return;

    const int count = m_listWidget ? m_listWidget->count() : 0;
    m_countLabel->setText(QStringLiteral("%1 item%2").arg(count).arg(count == 1 ? "" : "s"));
}
