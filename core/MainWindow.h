#pragma once

#include <QMainWindow>

class RenderWidget;
class WaveformWidget;
class TimelineWidget;
class TransportController;
class TimelineViewState;
class AnalysisManager;
class AssetManager;
class AssetBrowser;
class PanelManager;
class SelectionModel;
class InspectorWidget;
class QDockWidget;
class QLabel;
namespace Ui { class MainWindow; }

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

protected:
    void closeEvent(QCloseEvent* event) override;

private:
    void setupGlobalToolbar();
    void setupDocks();

    Ui::MainWindow* m_ui = nullptr;
    RenderWidget* m_renderWidget = nullptr;
    WaveformWidget* m_waveformWidget = nullptr;
    TimelineWidget* m_timelineWidget = nullptr;
    TransportController* m_transport = nullptr;
    TimelineViewState* m_viewState = nullptr;
    AnalysisManager* m_analysisManager = nullptr;
    QLabel* m_timeLabel = nullptr;

    AssetManager* m_assetManager = nullptr;
    AssetBrowser* m_assetBrowser = nullptr;
    PanelManager* m_panelManager = nullptr;
    SelectionModel* m_selectionModel = nullptr;
    InspectorWidget* m_inspectorWidget = nullptr;

    QDockWidget* m_assetDock = nullptr;
    QDockWidget* m_inspectorDock = nullptr;
    QDockWidget* m_timelineDock = nullptr;
};
