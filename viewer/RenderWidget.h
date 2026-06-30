#pragma once

#include <QOpenGLBuffer>
#include <QOpenGLFunctions_3_3_Core>
#include <QOpenGLShaderProgram>
#include <QOpenGLVertexArrayObject>
#include <QOpenGLWidget>

#include "Mesh.h"

class RenderWidget : public QOpenGLWidget, protected QOpenGLFunctions_3_3_Core
{
    Q_OBJECT

public:
    explicit RenderWidget(QWidget* parent = nullptr);

    bool loadStl(const QString& path, QString* errorMessage = nullptr);
    void resetView();

protected:
    void initializeGL() override;
    void paintGL() override;
    void resizeGL(int width, int height) override;

    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;

    void dragEnterEvent(QDragEnterEvent* event) override;
    void dropEvent(QDropEvent* event) override;

private:
    void updateMeshBuffer();

    QOpenGLShaderProgram m_program;
    QOpenGLBuffer m_vbo;
    QOpenGLVertexArrayObject m_vao;
    Mesh m_mesh;
    bool m_hasMesh = false;

    float m_yaw = 0.0f;
    float m_pitch = -15.0f;
    float m_distance = 2.5f;
    QVector3D m_pan;

    QPoint m_lastPos;
    Qt::MouseButton m_activeButton = Qt::NoButton;
};
