/*
 * SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Reprise Notation contributors
 */
#include "repriseclefhelper.h"

using namespace mu::palette;

RepriseClefHelper::RepriseClefHelper(QObject* parent)
    : QAbstractListModel(parent), muse::Contextable(muse::iocCtxForQmlObject(this))
{
}

void RepriseClefHelper::classBegin()
{
    auto* provider = dynamic_cast<PaletteProvider*>(m_paletteProvider().get());
    if (!provider) {
        return;
    }

    auto* catalogModel = qobject_cast<PaletteTreeModel*>(provider->catalogPaletteModel());
    if (!catalogModel) {
        return;
    }

    m_provider = provider;
    m_catalogModel = catalogModel;

    connect(provider, &QObject::destroyed, this, &RepriseClefHelper::invalidate);
    connect(m_catalogModel.data(), &QObject::destroyed, this, &RepriseClefHelper::invalidate);

    rebuild();
}

int RepriseClefHelper::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : static_cast<int>(m_entries.size());
}

QVariant RepriseClefHelper::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.parent().isValid() || index.column() != 0
        || index.row() < 0 || index.row() >= rowCount()) {
        return {};
    }

    const QPersistentModelIndex& sourceIndex = m_entries.at(static_cast<size_t>(index.row()));
    return sourceIndex.isValid() && sourceIndex.model()
           ? sourceIndex.model()->data(sourceIndex, role) : QVariant();
}

QHash<int, QByteArray> RepriseClefHelper::roleNames() const
{
    return m_catalogModel ? m_catalogModel->roleNames() : QAbstractListModel::roleNames();
}

bool RepriseClefHelper::available() const
{
    return m_provider && m_catalogModel && !m_entries.empty();
}

int RepriseClefHelper::count() const
{
    return rowCount();
}

bool RepriseClefHelper::apply(int row)
{
    if (!m_provider || row < 0 || row >= rowCount()) {
        return false;
    }

    const QPersistentModelIndex& index = m_entries.at(static_cast<size_t>(row));
    AbstractPaletteController* controller = m_provider->catalogPaletteController();
    return index.isValid() && controller
           && controller->applyPaletteElement(index, Qt::NoModifier);
}

QModelIndex RepriseClefHelper::catalogPaletteIndex() const
{
    if (!m_provider || !m_catalogModel) {
        return {};
    }

    const QModelIndex index = m_provider->catalogPaletteIndex(QStringLiteral("clefs"));
    return index.isValid() && index.model() == m_catalogModel ? index : QModelIndex();
}

void RepriseClefHelper::rebuild()
{
    beginResetModel();
    m_entries.clear();

    if (m_catalogModel) {
        const QModelIndex root = catalogPaletteIndex();
        if (root.isValid()) {
            const int rows = m_catalogModel->rowCount(root);
            for (int row = 0; row < rows; ++row) {
                const QModelIndex index = m_catalogModel->index(row, 0, root);
                if (m_catalogModel->findCell(index)) {
                    m_entries.emplace_back(index);
                }
            }
        }
    }

    endResetModel();
    emit modelChanged();
}

void RepriseClefHelper::invalidate()
{
    beginResetModel();
    m_entries.clear();
    m_provider.clear();
    m_catalogModel.clear();
    endResetModel();
    emit modelChanged();
}
