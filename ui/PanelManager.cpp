#include "PanelManager.h"

#include <QMainWindow>
#include <QDockWidget>
#include <QToolButton>
#include <QSettings>

PanelManager::PanelManager(QMainWindow* mainWindow, QObject* parent)
    : QObject(parent)
    , m_mainWindow(mainWindow)
{
}

PanelManager::~PanelManager() = default;

void PanelManager::registerPanel(PanelID id, QDockWidget* dock, QToolButton* toggleBtn)
{
    m_panels[id] = {dock, toggleBtn};

    if (toggleBtn)
    {
        toggleBtn->setChecked(dock->isVisible());
        connect(toggleBtn, &QToolButton::toggled, this, [this, id](bool checked) {
            if (checked) findDock(id)->show();
            else findDock(id)->hide();
        });
    }

    connect(dock, &QDockWidget::visibilityChanged, this, [this, id](bool visible) {
        auto entry = m_panels.find(id);
        if (entry != m_panels.end())
        {
            if (entry.value().toggleBtn)
                entry.value().toggleBtn->setChecked(visible);
            emit panelVisibilityChanged(id, visible);
        }
    });
}

void PanelManager::showPanel(PanelID id)
{
    if (auto* d = findDock(id))
        d->show();
}

void PanelManager::hidePanel(PanelID id)
{
    if (auto* d = findDock(id))
        d->hide();
}

void PanelManager::togglePanel(PanelID id)
{
    if (auto* d = findDock(id))
    {
        if (d->isVisible()) d->hide();
        else d->show();
    }
}

bool PanelManager::isPanelVisible(PanelID id) const
{
    if (auto* d = findDock(id))
        return d->isVisible();
    return false;
}

QDockWidget* PanelManager::dock(PanelID id) const
{
    return findDock(id);
}

QDockWidget* PanelManager::findDock(PanelID id) const
{
    auto it = m_panels.find(id);
    return (it != m_panels.end()) ? it->dock : nullptr;
}

void PanelManager::saveLayout()
{
    QSettings settings("KaleidoSound", "KaleidoSound");

    // Save visibility
    for (auto it = m_panels.constBegin(); it != m_panels.constEnd(); ++it)
    {
        QString key = QString("panel/%1/visible").arg(static_cast<int>(it.key()));
        settings.setValue(key, it.value().dock->isVisible());
    }

    // Save main window geometry
    settings.setValue("window/geometry", m_mainWindow->saveGeometry());
    settings.setValue("window/state", m_mainWindow->saveState());
}

void PanelManager::restoreLayout()
{
    QSettings settings("KaleidoSound", "KaleidoSound");

    // Restore window state (geometry + dock positions)
    if (settings.contains("window/state"))
    {
        m_mainWindow->restoreGeometry(settings.value("window/geometry").toByteArray());
        m_mainWindow->restoreState(settings.value("window/state").toByteArray());
    }

    // Restore panel visibility (override state if saved)
    for (auto it = m_panels.constBegin(); it != m_panels.constEnd(); ++it)
    {
        QString key = QString("panel/%1/visible").arg(static_cast<int>(it.key()));
        if (settings.contains(key))
        {
            bool visible = settings.value(key).toBool();
            if (visible) it.value().dock->show();
            else it.value().dock->hide();
        }
    }
}

void PanelManager::resetLayout()
{
    QSettings settings("KaleidoSound", "KaleidoSound");
    settings.remove("window/geometry");
    settings.remove("window/state");
    for (auto it = m_panels.constBegin(); it != m_panels.constEnd(); ++it)
        settings.remove(QString("panel/%1/visible").arg(static_cast<int>(it.key())));
}
