#pragma once

#include <QWidget>
#include <QVBoxLayout>
#include <QLabel>
#include <QMap>
#include "../asset/SelectionModel.h"

class AssetManager;
class AssetManager;

class InspectorWidget : public QWidget
{
    Q_OBJECT

public:
    explicit InspectorWidget(SelectionModel* selection, AssetManager* assets, QWidget* parent = nullptr);

private slots:
    void onSelectionChanged(SelectableType type, const QString& id);
    void onSelectionCleared();

private:
    void showAudioInspector(const QString& assetId);
    void showSTLInspector(const QString& assetId);
    void showPlaceholder();
    void clearContent();

    SelectionModel* m_selection = nullptr;
    AssetManager* m_assets = nullptr;
    QVBoxLayout* m_layout = nullptr;
    QLabel* m_titleLabel = nullptr;
    QWidget* m_contentWidget = nullptr;
    QVBoxLayout* m_contentLayout = nullptr;
};
