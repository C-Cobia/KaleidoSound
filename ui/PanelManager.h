#pragma once

#include <QObject>
#include <QMap>
#include <QString>

class QMainWindow;
class QDockWidget;
class QToolButton;
class QWidget;

enum class PanelID
{
    AssetBrowser,
    Inspector,
    Timeline,
    Viewer,
    Spectrogram,
    Console
};

class PanelManager : public QObject
{
    Q_OBJECT

public:
    explicit PanelManager(QMainWindow* mainWindow, QObject* parent = nullptr);
    ~PanelManager() override;

    // Register a panel
    void registerPanel(PanelID id, QDockWidget* dock, QToolButton* toggleBtn = nullptr);

    // Panel operations
    void showPanel(PanelID id);
    void hidePanel(PanelID id);
    void togglePanel(PanelID id);
    bool isPanelVisible(PanelID id) const;

    QDockWidget* dock(PanelID id) const;

    // Layout persistence
    void saveLayout();
    void restoreLayout();
    void resetLayout();

signals:
    void panelVisibilityChanged(PanelID id, bool visible);

private:
    QDockWidget* findDock(PanelID id) const;

    QMainWindow* m_mainWindow = nullptr;
    struct PanelEntry {
        QDockWidget* dock = nullptr;
        QToolButton* toggleBtn = nullptr;
    };
    QMap<PanelID, PanelEntry> m_panels;
};
