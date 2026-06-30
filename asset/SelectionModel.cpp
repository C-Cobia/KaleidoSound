#include "SelectionModel.h"

SelectionModel::SelectionModel(QObject* parent)
    : QObject(parent)
{
}

void SelectionModel::select(SelectableType type, const QString& id)
{
    if (m_type == type && m_id == id) return;
    m_type = type;
    m_id = id;
    emit selectionChanged(type, id);
}

void SelectionModel::clearSelection()
{
    if (m_type == SelectableType::None) return;
    m_type = SelectableType::None;
    m_id.clear();
    emit selectionCleared();
}
