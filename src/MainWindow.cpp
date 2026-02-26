#include "MainWindow.h"

#include <QFileDialog>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QToolButton>
#include <QVBoxLayout>
#include <QWidget>

#include "RenderWidget.h"

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
{
    createLayout();
    setWindowTitle(QStringLiteral("KaleidoSound"));
    resize(1200, 800);
}

void MainWindow::createLayout()
{
    QWidget* central = new QWidget(this);
    setCentralWidget(central);

    QHBoxLayout* mainLayout = new QHBoxLayout(central);
    mainLayout->setContentsMargins(16, 16, 16, 16);
    mainLayout->setSpacing(16);

    QWidget* sideBar = new QWidget(central);
    sideBar->setObjectName(QStringLiteral("sideBar"));
    sideBar->setFixedWidth(220);
    QVBoxLayout* sideLayout = new QVBoxLayout(sideBar);
    sideLayout->setSpacing(12);
    sideLayout->setContentsMargins(12, 12, 12, 12);

    QLabel* brand = new QLabel(QStringLiteral("KaleidoSound"), sideBar);
    QFont brandFont = brand->font();
    brandFont.setPointSize(14);
    brandFont.setBold(true);
    brand->setFont(brandFont);

    sideLayout->addWidget(brand);
    sideLayout->addSpacing(8);

    QStringList navItems = {QStringLiteral("Home"), QStringLiteral("Assets"), QStringLiteral("Viewer"), QStringLiteral("Settings")};
    for (const QString& item : navItems)
    {
        QPushButton* button = new QPushButton(item, sideBar);
        button->setMinimumHeight(36);
        sideLayout->addWidget(button);
    }
    sideLayout->addStretch();

    QWidget* rightPanel = new QWidget(central);
    QVBoxLayout* rightLayout = new QVBoxLayout(rightPanel);
    rightLayout->setSpacing(12);
    rightLayout->setContentsMargins(0, 0, 0, 0);

    QWidget* topBar = new QWidget(rightPanel);
    QHBoxLayout* topLayout = new QHBoxLayout(topBar);
    topLayout->setContentsMargins(12, 8, 12, 8);
    topLayout->setSpacing(8);

    QLabel* title = new QLabel(QStringLiteral("Model Viewer"), topBar);
    QFont titleFont = title->font();
    titleFont.setPointSize(14);
    titleFont.setBold(true);
    title->setFont(titleFont);

    topLayout->addWidget(title);
    topLayout->addStretch();

    QToolButton* openButton = new QToolButton(topBar);
    openButton->setText(QStringLiteral("Open STL"));
    QToolButton* resetButton = new QToolButton(topBar);
    resetButton->setText(QStringLiteral("Reset View"));
    topLayout->addWidget(openButton);
    topLayout->addWidget(resetButton);

    QWidget* viewerCard = new QWidget(rightPanel);
    viewerCard->setObjectName(QStringLiteral("viewerCard"));
    QVBoxLayout* viewerLayout = new QVBoxLayout(viewerCard);
    viewerLayout->setContentsMargins(12, 12, 12, 12);

    m_renderWidget = new RenderWidget(viewerCard);
    viewerLayout->addWidget(m_renderWidget);

    rightLayout->addWidget(topBar);
    rightLayout->addWidget(viewerCard, 1);

    mainLayout->addWidget(sideBar);
    mainLayout->addWidget(rightPanel, 1);

    connect(openButton, &QToolButton::clicked, this, [this]() {
        const QString filePath = QFileDialog::getOpenFileName(this, QStringLiteral("Open STL"), QString(), QStringLiteral("STL Files (*.stl)"));
        if (filePath.isEmpty())
        {
            return;
        }
        QString errorMessage;
        if (!m_renderWidget->loadStl(filePath, &errorMessage))
        {
            QMessageBox::warning(this, QStringLiteral("Load Failed"), errorMessage.isEmpty() ? QStringLiteral("Failed to load STL.") : errorMessage);
        }
    });

    connect(resetButton, &QToolButton::clicked, this, [this]() {
        m_renderWidget->resetView();
    });
}
