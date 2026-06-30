#include "MainWindow.h"

#include <QDockWidget>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QMessageBox>
#include <QPushButton>
#include <QLabel>
#include <QFont>
#include <QMenu>
#include <QAction>
#include <QCloseEvent>
#include <QCoreApplication>

#include "../viewer/RenderWidget.h"
#include "../timeline/WaveformWidget.h"
#include "../timeline/TimelineWidget.h"
#include "../timeline/WaveformTrack.h"
#include "../timeline/BeatTrack.h"
#include "../audio/TransportController.h"
#include "../audio/AnalysisManager.h"
#include "../audio/AubioBeatDetector.h"
#include "../audio/BeatNetDetector.h"
#include "../timeline/TimelineViewState.h"
#include "../asset/AssetManager.h"
#include "../asset/AssetBrowser.h"
#include "../ui/PanelManager.h"
#include "../asset/SelectionModel.h"
#include "../ui/InspectorWidget.h"
#include "ui_MainWindow.h"

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
    , m_ui(new Ui::MainWindow)
{
    m_ui->setupUi(this);

    // 1. Core systems
    m_assetManager = new AssetManager(this);
    m_transport = new TransportController(this);
    m_viewState = new TimelineViewState(this);
    m_viewState->setTransport(m_transport);
    m_selectionModel = new SelectionModel(this);
    m_panelManager = new PanelManager(this, this);

    // Analysis system — pluggable detectors
    m_analysisManager = new AnalysisManager(this);
    m_analysisManager->registerDetector(new AubioBeatDetector());

    auto* beatnetDetector = new BeatNetDetector();
    // Try to load BeatNet model
    QString appDir = QCoreApplication::applicationDirPath();
    QStringList modelPaths = {
        appDir + "/Third-Parties/beatnet-models/beatnet_crnn.pt",
        appDir + "/../Third-Parties/beatnet-models/beatnet_crnn.pt",
        appDir + "/../../Third-Parties/beatnet-models/beatnet_crnn.pt",
    };
    for (const QString& path : modelPaths)
    {
        if (QFile::exists(path) && beatnetDetector->loadModel(path))
        {
            qInfo("MainWindow: BeatNet model loaded from %s", qPrintable(path));
            break;
        }
    }
    if (!beatnetDetector->isAvailable())
        qWarning("MainWindow: BeatNet model not available; falling back to aubio");
    m_analysisManager->registerDetector(beatnetDetector);
    m_assetManager->setAnalysisManager(m_analysisManager);

    // 2. Global toolbar
    setupGlobalToolbar();

    // 3. RenderWidget in center
    auto* viewerLayout = new QVBoxLayout(m_ui->renderWidgetContainer);
    viewerLayout->setContentsMargins(0, 0, 0, 0);
    m_renderWidget = new RenderWidget(m_ui->renderWidgetContainer);
    viewerLayout->addWidget(m_renderWidget);

    // 4. Dockable panels
    setupDocks();

    // 5. Playback time label — driven by TransportController
    m_timeLabel = m_ui->playbackTimeLabel;
    m_timeLabel->setStyleSheet("color: #00c8f0; background: transparent; padding: 0 8px;");
    m_timeLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    m_timeLabel->hide();

    auto formatTime = [](double sec) -> QString {
        int total = static_cast<int>(sec);
        int h = total / 3600;
        int m = (total % 3600) / 60;
        int s = total % 60;
        if (h > 0)
            return QStringLiteral("%1:%2:%3").arg(h).arg(m, 2, 10, QChar('0')).arg(s, 2, 10, QChar('0'));
        return QStringLiteral("%1:%2").arg(m, 2, 10, QChar('0')).arg(s, 2, 10, QChar('0'));
    };

    connect(m_transport, &TransportController::positionChanged, this,
        [this, formatTime](double sec) {
            double dur = m_transport->totalDuration();
            m_timeLabel->setText(QStringLiteral("%1 / %2").arg(formatTime(sec)).arg(formatTime(dur)));
            m_timeLabel->setVisible(dur > 0);
        });

    // 6. Asset selection -> load + inspector update
    connect(m_assetBrowser, &AssetBrowser::assetSelected, this, [this](const QString& assetId) {
        if (m_assetManager->hasSTL(assetId))
        {
            m_selectionModel->select(SelectableType::STLAsset, assetId);
            STLAsset* stl = m_assetManager->stlAsset(assetId);
            if (stl)
            {
                QString errorMessage;
                if (!m_renderWidget->loadStl(stl->filePath, &errorMessage))
                    QMessageBox::warning(this, QStringLiteral("Load Failed"), errorMessage);
            }
        }
        else if (m_assetManager->hasAudio(assetId))
        {
            m_selectionModel->select(SelectableType::AudioAsset, assetId);
            AudioAsset* audio = m_assetManager->audioAsset(assetId);
            if (audio && audio->analyzed)
            {
                m_waveformWidget->setAudioData(audio->data, audio->beats, audio->filePath);
                m_timelineDock->show();
            }
        }
    });

    // 7. Auto-load audio when finished
    connect(m_assetManager, &AssetManager::audioLoaded, this, [this](const QString& assetId) {
        AudioAsset* audio = m_assetManager->audioAsset(assetId);
        if (audio)
        {
            m_waveformWidget->setAudioData(audio->data, audio->beats, audio->filePath);

            // Setup TimelineWidget tracks
            m_timelineWidget->clearTracks();

            auto* waveTrack = new WaveformTrack();
            waveTrack->setAudioData(audio->data);
            m_timelineWidget->addTrack(waveTrack);

            auto* beatTrack = new BeatTrack();
            beatTrack->setBeatData(audio->beats);
            m_timelineWidget->addTrack(beatTrack);

            // Pass beat times to TimelineWidget for metronome
            m_timelineWidget->setBeatTimes(audio->beats.beatTimes());

            m_timelineDock->show();
        }
    });

    // 8. Restore saved layout
    m_panelManager->restoreLayout();
}

MainWindow::~MainWindow()
{
    m_panelManager->saveLayout();
    delete m_ui;
}

void MainWindow::closeEvent(QCloseEvent* event)
{
    m_panelManager->saveLayout();
    QMainWindow::closeEvent(event);
}

// ============================================================================
// Global Toolbar
// ============================================================================

void MainWindow::setupGlobalToolbar()
{
    auto* importMenu = new QMenu(this);
    auto* importAudioAction = importMenu->addAction("Import Audio...");
    auto* importSTLAction = importMenu->addAction("Import STL...");
    m_ui->importButton->setMenu(importMenu);

    connect(importAudioAction, &QAction::triggered, this, [this]() {
        const QString filePath = QFileDialog::getOpenFileName(
            this, QStringLiteral("Import Audio"), QString(),
            QStringLiteral("Audio Files (*.wav *.mp3 *.flac *.ogg *.aac *.wma);;All Files (*.*)"));
        if (!filePath.isEmpty())
            m_assetManager->addAudio(filePath);
    });

    connect(importSTLAction, &QAction::triggered, this, [this]() {
        const QString filePath = QFileDialog::getOpenFileName(
            this, QStringLiteral("Import STL"), QString(), QStringLiteral("STL Files (*.stl)"));
        if (filePath.isEmpty()) return;
        m_assetManager->addSTL(filePath);
        QString errorMessage;
        if (!m_renderWidget->loadStl(filePath, &errorMessage))
            QMessageBox::warning(this, QStringLiteral("Load Failed"), errorMessage);
    });
}

// ============================================================================
// Dockable Panels
// ============================================================================

void MainWindow::setupDocks()
{
    // --- Asset Browser Dock (Left) ---
    m_assetDock = new QDockWidget(QStringLiteral("Project"), this);
    m_assetDock->setObjectName("assetDock");
    m_assetDock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    m_assetDock->setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable | QDockWidget::DockWidgetClosable);

    m_assetBrowser = new AssetBrowser(m_assetManager);
    m_assetDock->setWidget(m_assetBrowser);
    addDockWidget(Qt::LeftDockWidgetArea, m_assetDock);

    connect(m_assetBrowser, &AssetBrowser::importRequested, this, [this]() {
        const QString filePath = QFileDialog::getOpenFileName(
            this, QStringLiteral("Import Audio"), QString(),
            QStringLiteral("Audio Files (*.wav *.mp3 *.flac *.ogg *.aac *.wma);;All Files (*.*)"));
        if (!filePath.isEmpty())
            m_assetManager->addAudio(filePath);
    });

    m_panelManager->registerPanel(PanelID::AssetBrowser, m_assetDock, m_ui->toggleProjectBtn);

    // --- Inspector Dock (Right) ---
    m_inspectorDock = new QDockWidget(QStringLiteral("Inspector"), this);
    m_inspectorDock->setObjectName("inspectorDock");
    m_inspectorDock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    m_inspectorDock->setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable | QDockWidget::DockWidgetClosable);

    m_inspectorWidget = new InspectorWidget(m_selectionModel, m_assetManager);
    m_inspectorDock->setWidget(m_inspectorWidget);
    addDockWidget(Qt::RightDockWidgetArea, m_inspectorDock);

    m_panelManager->registerPanel(PanelID::Inspector, m_inspectorDock, m_ui->toggleInspectorBtn);

    // --- Timeline Dock (Bottom) ---
    m_timelineDock = new QDockWidget(QStringLiteral("Timeline"), this);
    m_timelineDock->setObjectName("timelineDock");
    m_timelineDock->setAllowedAreas(Qt::BottomDockWidgetArea | Qt::TopDockWidgetArea);
    m_timelineDock->setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable | QDockWidget::DockWidgetClosable);

    auto* transportBar = new QWidget;
    transportBar->setObjectName("audioToolbar");
    transportBar->setFixedHeight(32);
    auto* transportLayout = new QHBoxLayout(transportBar);
    transportLayout->setContentsMargins(8, 0, 8, 0);
    transportLayout->setSpacing(4);

    auto* playBtn = new QPushButton("Play");
    playBtn->setObjectName("playButton");
    playBtn->setCursor(Qt::PointingHandCursor);

    auto* stopBtn = new QPushButton("Stop");
    stopBtn->setObjectName("stopButton");
    stopBtn->setCursor(Qt::PointingHandCursor);

    auto* transportSpacer = new QWidget;
    transportSpacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

    transportLayout->addWidget(playBtn);
    transportLayout->addWidget(stopBtn);
    transportLayout->addWidget(transportSpacer);

    m_waveformWidget = new WaveformWidget;
    m_waveformWidget->setTransport(m_transport);
    m_waveformWidget->setTimelineState(m_viewState);

    // New TimelineWidget with Track system
    m_timelineWidget = new TimelineWidget;
    m_timelineWidget->setTransport(m_transport);
    m_timelineWidget->setViewState(m_viewState);

    auto* container = new QWidget;
    auto* layout = new QVBoxLayout(container);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(transportBar);
    layout->addWidget(m_timelineWidget, 1);

    m_timelineDock->setWidget(container);
    addDockWidget(Qt::BottomDockWidgetArea, m_timelineDock);
    m_timelineDock->hide();

    m_panelManager->registerPanel(PanelID::Timeline, m_timelineDock, m_ui->toggleTimelineBtn);

    // Transport controls — all through TransportController
    connect(playBtn, &QPushButton::clicked, this, [this, playBtn]() {
        if (m_transport->isPlaying())
        {
            m_transport->pause();
            playBtn->setText("Play");
        }
        else
        {
            m_transport->play();
            playBtn->setText("Pause");
        }
    });

    connect(stopBtn, &QPushButton::clicked, this, [this, playBtn]() {
        m_transport->stop();
        playBtn->setText("Play");
    });

    // Wire TransportController -> WaveformWidget (private slot, use lambda)
    connect(m_transport, &TransportController::positionChanged, m_waveformWidget, [this](double sec) {
        m_waveformWidget->onPositionChanged(sec);
    });
    connect(m_transport, &TransportController::playbackStopped, m_waveformWidget, [this]() {
        m_waveformWidget->update();
    });

    // Wire TransportController -> TimelineWidget
    connect(m_transport, &TransportController::positionChanged, m_timelineWidget, &TimelineWidget::onPositionChanged);

    // Set initial sizes
    resizeDocks({m_assetDock}, {260}, Qt::Horizontal);
    resizeDocks({m_inspectorDock}, {280}, Qt::Horizontal);
    resizeDocks({m_timelineDock}, {200}, Qt::Vertical);
}
