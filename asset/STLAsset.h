#pragma once

#include <QString>
#include <QUuid>

struct STLAsset
{
    QString id;
    QString name;
    QString filePath;

    static STLAsset create(const QString& filePath)
    {
        STLAsset a;
        a.id = QUuid::createUuid().toString();
        a.filePath = filePath;
        a.name = filePath.section('/', -1).section('\\', -1);
        return a;
    }
};
