#pragma once

#include <QMainWindow>

class RenderWidget;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);

private:
    void createLayout();

    RenderWidget* m_renderWidget = nullptr;
};
