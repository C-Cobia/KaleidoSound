#pragma once

#include <QObject>
#include <QString>

enum class SelectableType
{
    None,
    AudioAsset,
    STLAsset,
    BeatMarker,
    SpectrogramRegion
};

class SelectionModel : public QObject
{
    Q_OBJECT

public:
    explicit SelectionModel(QObject* parent = nullptr);

    void select(SelectableType type, const QString& id);
    void clearSelection();

    SelectableType selectedType() const { return m_type; }
    QString selectedId() const { return m_id; }
    bool hasSelection() const { return m_type != SelectableType::None; }

signals:
    void selectionChanged(SelectableType type, const QString& id);
    void selectionCleared();

private:
    SelectableType m_type = SelectableType::None;
    QString m_id;
};
