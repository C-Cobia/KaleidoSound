#include "Mesh.h"

#include <algorithm>

namespace
{
void appendVertex(Mesh& mesh, const QVector3D& position, const QVector3D& normal)
{
    mesh.vertices.push_back(position.x());
    mesh.vertices.push_back(position.y());
    mesh.vertices.push_back(position.z());
    mesh.vertices.push_back(normal.x());
    mesh.vertices.push_back(normal.y());
    mesh.vertices.push_back(normal.z());
}

void addTriangle(Mesh& mesh, const QVector3D& a, const QVector3D& b, const QVector3D& c)
{
    QVector3D normal = QVector3D::normal(a, b, c);
    appendVertex(mesh, a, normal);
    appendVertex(mesh, b, normal);
    appendVertex(mesh, c, normal);
}

void addQuad(Mesh& mesh, const QVector3D& a, const QVector3D& b, const QVector3D& c, const QVector3D& d)
{
    addTriangle(mesh, a, b, c);
    addTriangle(mesh, a, c, d);
}

void addBox(Mesh& mesh, const QVector3D& center, const QVector3D& size)
{
    QVector3D h = size * 0.5f;

    QVector3D p000 = center + QVector3D(-h.x(), -h.y(), -h.z());
    QVector3D p001 = center + QVector3D(-h.x(), -h.y(), h.z());
    QVector3D p010 = center + QVector3D(-h.x(), h.y(), -h.z());
    QVector3D p011 = center + QVector3D(-h.x(), h.y(), h.z());
    QVector3D p100 = center + QVector3D(h.x(), -h.y(), -h.z());
    QVector3D p101 = center + QVector3D(h.x(), -h.y(), h.z());
    QVector3D p110 = center + QVector3D(h.x(), h.y(), -h.z());
    QVector3D p111 = center + QVector3D(h.x(), h.y(), h.z());

    addQuad(mesh, p100, p101, p111, p110);
    addQuad(mesh, p000, p010, p011, p001);
    addQuad(mesh, p010, p110, p111, p011);
    addQuad(mesh, p000, p001, p101, p100);
    addQuad(mesh, p001, p011, p111, p101);
    addQuad(mesh, p000, p100, p110, p010);
}
} // namespace

void normalizeMesh(Mesh& mesh)
{
    if (mesh.vertices.empty())
    {
        return;
    }

    QVector3D minV(mesh.vertices[0], mesh.vertices[1], mesh.vertices[2]);
    QVector3D maxV = minV;

    for (size_t i = 0; i < mesh.vertices.size(); i += 6)
    {
        QVector3D p(mesh.vertices[i], mesh.vertices[i + 1], mesh.vertices[i + 2]);
        minV.setX(std::min(minV.x(), p.x()));
        minV.setY(std::min(minV.y(), p.y()));
        minV.setZ(std::min(minV.z(), p.z()));
        maxV.setX(std::max(maxV.x(), p.x()));
        maxV.setY(std::max(maxV.y(), p.y()));
        maxV.setZ(std::max(maxV.z(), p.z()));
    }

    QVector3D center = (minV + maxV) * 0.5f;
    QVector3D extent = maxV - minV;
    float maxExtent = std::max({extent.x(), extent.y(), extent.z(), 1e-6f});
    float scale = 1.0f / maxExtent;

    for (size_t i = 0; i < mesh.vertices.size(); i += 6)
    {
        QVector3D p(mesh.vertices[i], mesh.vertices[i + 1], mesh.vertices[i + 2]);
        p = (p - center) * scale;
        mesh.vertices[i] = p.x();
        mesh.vertices[i + 1] = p.y();
        mesh.vertices[i + 2] = p.z();
    }
}

Mesh createDefaultBunny()
{
    Mesh mesh;
    addBox(mesh, QVector3D(0.0f, -0.1f, 0.0f), QVector3D(1.4f, 0.8f, 0.7f));
    addBox(mesh, QVector3D(0.7f, 0.2f, 0.0f), QVector3D(0.6f, 0.5f, 0.5f));
    addBox(mesh, QVector3D(-0.7f, -0.05f, 0.0f), QVector3D(0.3f, 0.3f, 0.3f));
    addBox(mesh, QVector3D(0.8f, 0.85f, 0.15f), QVector3D(0.18f, 0.7f, 0.12f));
    addBox(mesh, QVector3D(0.8f, 0.85f, -0.15f), QVector3D(0.18f, 0.7f, 0.12f));

    normalizeMesh(mesh);
    return mesh;
}
