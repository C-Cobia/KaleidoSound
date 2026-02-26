#include "RenderWidget.h"

#include <QDragEnterEvent>
#include <QFileInfo>
#include <QMatrix4x4>
#include <QMimeData>
#include <QMouseEvent>
#include <QUrl>
#include <QWheelEvent>

#include <algorithm>

#include "StlLoader.h"

RenderWidget::RenderWidget(QWidget* parent)
    : QOpenGLWidget(parent)
    , m_vbo(QOpenGLBuffer::VertexBuffer)
{
    setFocusPolicy(Qt::StrongFocus);
    setAcceptDrops(true);
    m_mesh = createDefaultBunny();
    m_hasMesh = true;
}

bool RenderWidget::loadStl(const QString& path, QString* errorMessage)
{
    Mesh mesh;
    if (!loadBinaryStl(path, mesh, errorMessage))
    {
        return false;
    }

    m_mesh = std::move(mesh);
    m_hasMesh = true;
    updateMeshBuffer();
    update();
    return true;
}

void RenderWidget::resetView()
{
    m_yaw = 0.0f;
    m_pitch = -15.0f;
    m_distance = 2.5f;
    m_pan = QVector3D();
    update();
}

void RenderWidget::initializeGL()
{
    initializeOpenGLFunctions();

    glEnable(GL_DEPTH_TEST);

    m_program.addShaderFromSourceCode(QOpenGLShader::Vertex,
                                      R"(
#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;

uniform mat4 uMvp;
uniform mat4 uModel;

out vec3 vNormal;

void main()
{
    vNormal = mat3(transpose(inverse(uModel))) * aNormal;
    gl_Position = uMvp * vec4(aPos, 1.0);
}
)");

    m_program.addShaderFromSourceCode(QOpenGLShader::Fragment,
                                      R"(
#version 330 core
in vec3 vNormal;

uniform vec3 uColor;

out vec4 FragColor;

void main()
{
    vec3 normal = normalize(vNormal);
    vec3 lightDir = normalize(vec3(0.4, 0.8, 0.6));
    float diff = max(dot(normal, lightDir), 0.15);
    vec3 color = uColor * diff;
    FragColor = vec4(color, 1.0);
}
)");

    m_program.link();

    m_vao.create();
    m_vao.bind();

    m_vbo.create();
    m_vbo.bind();
    m_vbo.setUsagePattern(QOpenGLBuffer::StaticDraw);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), reinterpret_cast<void*>(0));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), reinterpret_cast<void*>(3 * sizeof(float)));

    m_vbo.release();
    m_vao.release();

    updateMeshBuffer();
}

void RenderWidget::paintGL()
{
    glClearColor(0.96f, 0.97f, 0.98f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    if (!m_hasMesh)
    {
        return;
    }

    QMatrix4x4 projection;
    float aspect = height() == 0 ? 1.0f : static_cast<float>(width()) / static_cast<float>(height());
    projection.perspective(45.0f, aspect, 0.05f, 20.0f);

    QMatrix4x4 view;
    view.translate(m_pan);
    view.rotate(m_pitch, 1.0f, 0.0f, 0.0f);
    view.rotate(m_yaw, 0.0f, 1.0f, 0.0f);
    view.translate(0.0f, 0.0f, -m_distance);

    QMatrix4x4 model;
    QMatrix4x4 mvp = projection * view * model;

    m_program.bind();
    m_program.setUniformValue("uMvp", mvp);
    m_program.setUniformValue("uModel", model);
    m_program.setUniformValue("uColor", QVector3D(0.72f, 0.63f, 0.58f));

    m_vao.bind();
    glDrawArrays(GL_TRIANGLES, 0, m_mesh.vertexCount());
    m_vao.release();
    m_program.release();
}

void RenderWidget::resizeGL(int width, int height)
{
    glViewport(0, 0, width, height);
}

void RenderWidget::mousePressEvent(QMouseEvent* event)
{
    m_lastPos = event->pos();
    m_activeButton = event->button();
    event->accept();
}

void RenderWidget::mouseMoveEvent(QMouseEvent* event)
{
    QPoint delta = event->pos() - m_lastPos;
    m_lastPos = event->pos();

    if (m_activeButton == Qt::LeftButton)
    {
        m_yaw += delta.x() * 0.5f;
        m_pitch += delta.y() * 0.5f;
        m_pitch = std::clamp(m_pitch, -89.0f, 89.0f);
        update();
    }
    else if (m_activeButton == Qt::RightButton)
    {
        float panScale = 0.002f * m_distance;
        m_pan += QVector3D(delta.x() * panScale, -delta.y() * panScale, 0.0f);
        update();
    }

    event->accept();
}

void RenderWidget::mouseReleaseEvent(QMouseEvent* event)
{
    m_activeButton = Qt::NoButton;
    event->accept();
}

void RenderWidget::wheelEvent(QWheelEvent* event)
{
    QPoint numSteps = event->angleDelta() / 120;
    if (!numSteps.isNull())
    {
        m_distance -= static_cast<float>(numSteps.y()) * 0.15f;
        m_distance = std::clamp(m_distance, 0.4f, 10.0f);
        update();
    }
    event->accept();
}

void RenderWidget::dragEnterEvent(QDragEnterEvent* event)
{
    if (event->mimeData()->hasUrls())
    {
        const QList<QUrl> urls = event->mimeData()->urls();
        if (!urls.isEmpty() && urls.first().toLocalFile().endsWith(".stl", Qt::CaseInsensitive))
        {
            event->acceptProposedAction();
            return;
        }
    }

    event->ignore();
}

void RenderWidget::dropEvent(QDropEvent* event)
{
    const QList<QUrl> urls = event->mimeData()->urls();
    if (urls.isEmpty())
    {
        return;
    }

    const QString path = urls.first().toLocalFile();
    QString errorMessage;
    if (!loadStl(path, &errorMessage) && !errorMessage.isEmpty())
    {
        qWarning("%s", qPrintable(errorMessage));
    }

    event->acceptProposedAction();
}

void RenderWidget::updateMeshBuffer()
{
    if (!m_hasMesh || !m_vbo.isCreated())
    {
        return;
    }

    m_vbo.bind();
    m_vbo.allocate(m_mesh.vertices.data(), static_cast<int>(m_mesh.vertices.size() * sizeof(float)));
    m_vbo.release();
}
