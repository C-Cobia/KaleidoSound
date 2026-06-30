#include "InspectorWidget.h"
#include "../asset/SelectionModel.h"
#include "../asset/AssetManager.h"
#include "../asset/AudioAsset.h"
#include "../asset/STLAsset.h"

InspectorWidget::InspectorWidget(SelectionModel* selection, AssetManager* assets, QWidget* parent)
    : QWidget(parent)
    , m_selection(selection)
    , m_assets(assets)
{
    m_layout = new QVBoxLayout(this);
    m_layout->setContentsMargins(0, 0, 0, 0);
    m_layout->setSpacing(0);

    // Title
    m_titleLabel = new QLabel("Inspector");
    m_titleLabel->setObjectName("inspectorTitle");
    m_titleLabel->setFixedHeight(28);
    m_titleLabel->setStyleSheet(
        "QLabel { color: #888890; font-size: 10px; font-weight: bold; "
        "letter-spacing: 0.5px; padding: 0 12px; background: #2d2d30; "
        "border-bottom: 1px solid #333338; }");
    m_layout->addWidget(m_titleLabel);

    // Content area
    m_contentWidget = new QWidget;
    m_contentLayout = new QVBoxLayout(m_contentWidget);
    m_contentLayout->setContentsMargins(12, 12, 12, 12);
    m_contentLayout->setSpacing(8);
    m_layout->addWidget(m_contentWidget, 1);

    showPlaceholder();

    // Connect selection
    connect(m_selection, &SelectionModel::selectionChanged, this, &InspectorWidget::onSelectionChanged);
    connect(m_selection, &SelectionModel::selectionCleared, this, &InspectorWidget::onSelectionCleared);
}

void InspectorWidget::onSelectionChanged(SelectableType type, const QString& id)
{
    clearContent();
    if (type == SelectableType::AudioAsset) showAudioInspector(id);
    else if (type == SelectableType::STLAsset) showSTLInspector(id);
    else showPlaceholder();
}

void InspectorWidget::onSelectionCleared()
{
    clearContent();
    showPlaceholder();
}

void InspectorWidget::showAudioInspector(const QString& assetId)
{
    AudioAsset* asset = m_assets->audioAsset(assetId);
    if (!asset) { showPlaceholder(); return; }

    m_titleLabel->setText("Inspector — Audio");

    auto addRow = [this](const QString& label, const QString& value) {
        auto* row = new QWidget;
        auto* rowLayout = new QHBoxLayout(row);
        rowLayout->setContentsMargins(0, 0, 0, 0);
        rowLayout->setSpacing(8);

        auto* lbl = new QLabel(label);
        lbl->setFixedWidth(100);
        lbl->setStyleSheet("color: #6a6a72; font-size: 11px; background: transparent;");
        rowLayout->addWidget(lbl);

        auto* val = new QLabel(value);
        val->setStyleSheet("color: #d4d4d8; font-size: 11px; background: transparent;");
        val->setWordWrap(true);
        rowLayout->addWidget(val, 1);

        m_contentLayout->addWidget(row);
    };

    addRow("Name", asset->name);
    addRow("Duration", QStringLiteral("%1 s").arg(asset->duration, 0, 'f', 2));
    addRow("BPM", asset->analyzed ? QStringLiteral("%1").arg(asset->bpm, 0, 'f', 1) : "—");
    addRow("Sample Rate", QStringLiteral("%1 Hz").arg(asset->sampleRate));
    addRow("Channels", asset->channels == 1 ? "Mono" : "Stereo");
    addRow("Beat Count", asset->analyzed ? QStringLiteral("%1").arg(asset->beats.beats.size()) : "—");
    addRow("Status", asset->analyzed ? "Analyzed" : "Loading...");

    m_contentLayout->addStretch();
}

void InspectorWidget::showSTLInspector(const QString& assetId)
{
    STLAsset* asset = m_assets->stlAsset(assetId);
    if (!asset) { showPlaceholder(); return; }

    m_titleLabel->setText("Inspector — 3D Model");

    auto addRow = [this](const QString& label, const QString& value) {
        auto* row = new QWidget;
        auto* rowLayout = new QHBoxLayout(row);
        rowLayout->setContentsMargins(0, 0, 0, 0);
        rowLayout->setSpacing(8);

        auto* lbl = new QLabel(label);
        lbl->setFixedWidth(100);
        lbl->setStyleSheet("color: #6a6a72; font-size: 11px; background: transparent;");
        rowLayout->addWidget(lbl);

        auto* val = new QLabel(value);
        val->setStyleSheet("color: #d4d4d8; font-size: 11px; background: transparent;");
        val->setWordWrap(true);
        rowLayout->addWidget(val, 1);

        m_contentLayout->addWidget(row);
    };

    addRow("Name", asset->name);
    addRow("Path", asset->filePath);

    m_contentLayout->addStretch();
}

void InspectorWidget::showPlaceholder()
{
    m_titleLabel->setText("Inspector");
    auto* placeholder = new QLabel("No selection");
    placeholder->setAlignment(Qt::AlignCenter);
    placeholder->setStyleSheet("color: #606068; font-size: 11px; background: transparent;");
    m_contentLayout->addWidget(placeholder);
}

void InspectorWidget::clearContent()
{
    QLayoutItem* item;
    while ((item = m_contentLayout->takeAt(0)) != nullptr)
    {
        if (item->widget()) delete item->widget();
        if (item->layout()) delete item->layout();
        delete item;
    }
}
