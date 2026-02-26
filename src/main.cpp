#include <QApplication>

#include "MainWindow.h"

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    
    app.setStyleSheet(R"(
        QWidget { background-color: #f5f7fb; color: #1f2937; }
        QPushButton, QToolButton {
            background-color: #ffffff;
            border: 1px solid #e3e8f0;
            border-radius: 8px;
            padding: 6px 12px;
        }
        QPushButton:hover, QToolButton:hover { background-color: #f0f4ff; }
        QWidget#sideBar { background-color: #ffffff; border-radius: 12px; }
        QWidget#viewerCard { background-color: #ffffff; border-radius: 16px; }
    )");
    
    MainWindow window;
    window.show();

    return app.exec();
}
