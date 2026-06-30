#include "StlLoader.h"

#include <QByteArray>
#include <QFile>
#include <QVector3D>

#include <cstring>

namespace
{
bool readFloat(const QByteArray& data, int offset, float& value)
{
    if (offset + static_cast<int>(sizeof(float)) > data.size())
    {
        return false;
    }
    std::memcpy(&value, data.constData() + offset, sizeof(float));
    return true;
}
}

bool loadBinaryStl(const QString& path, Mesh& outMesh, QString* errorMessage)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
    {
        if (errorMessage)
        {
            *errorMessage = QStringLiteral("Failed to open STL file.");
        }
        return false;
    }

    QByteArray data = file.readAll();
    if (data.size() < 84)
    {
        if (errorMessage)
        {
            *errorMessage = QStringLiteral("Invalid STL file size.");
        }
        return false;
    }

    uint32_t triangleCount = 0;
    std::memcpy(&triangleCount, data.constData() + 80, sizeof(uint32_t));

    const int expectedSize = 84 + static_cast<int>(triangleCount) * 50;
    if (data.size() < expectedSize)
    {
        if (errorMessage)
        {
            *errorMessage = QStringLiteral("STL file is incomplete.");
        }
        return false;
    }

    Mesh mesh;
    mesh.vertices.reserve(static_cast<size_t>(triangleCount) * 18);

    int offset = 84;
    for (uint32_t i = 0; i < triangleCount; ++i)
    {
        float n[3]{};
        float v1[3]{};
        float v2[3]{};
        float v3[3]{};

        if (!readFloat(data, offset + 0, n[0]) ||
            !readFloat(data, offset + 4, n[1]) ||
            !readFloat(data, offset + 8, n[2]) ||
            !readFloat(data, offset + 12, v1[0]) ||
            !readFloat(data, offset + 16, v1[1]) ||
            !readFloat(data, offset + 20, v1[2]) ||
            !readFloat(data, offset + 24, v2[0]) ||
            !readFloat(data, offset + 28, v2[1]) ||
            !readFloat(data, offset + 32, v2[2]) ||
            !readFloat(data, offset + 36, v3[0]) ||
            !readFloat(data, offset + 40, v3[1]) ||
            !readFloat(data, offset + 44, v3[2]))
        {
            if (errorMessage)
            {
                *errorMessage = QStringLiteral("STL file parsing failed.");
            }
            return false;
        }

        QVector3D normal(n[0], n[1], n[2]);
        QVector3D p1(v1[0], v1[1], v1[2]);
        QVector3D p2(v2[0], v2[1], v2[2]);
        QVector3D p3(v3[0], v3[1], v3[2]);

        if (normal.lengthSquared() < 1e-8f)
        {
            normal = QVector3D::normal(p1, p2, p3);
        }
        else
        {
            normal.normalize();
        }

        mesh.vertices.insert(mesh.vertices.end(), {p1.x(), p1.y(), p1.z(), normal.x(), normal.y(), normal.z(),
                                                   p2.x(), p2.y(), p2.z(), normal.x(), normal.y(), normal.z(),
                                                   p3.x(), p3.y(), p3.z(), normal.x(), normal.y(), normal.z()});

        offset += 50;
    }

    normalizeMesh(mesh);
    outMesh = std::move(mesh);
    return true;
}
