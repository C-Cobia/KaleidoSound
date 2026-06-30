#pragma once

#include <QVector3D>
#include <vector>

struct Mesh
{
    std::vector<float> vertices;

    int vertexCount() const
    {
        return static_cast<int>(vertices.size() / 6);
    }
};

Mesh createDefaultBunny();
void normalizeMesh(Mesh& mesh);
