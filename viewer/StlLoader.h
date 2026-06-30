#pragma once

#include <QString>

#include "Mesh.h"

bool loadBinaryStl(const QString& path, Mesh& outMesh, QString* errorMessage = nullptr);
