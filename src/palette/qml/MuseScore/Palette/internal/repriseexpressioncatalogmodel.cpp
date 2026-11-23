/*
 * SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Reprise Notation contributors
 */
#include "repriseexpressioncatalogmodel.h"

#include "engraving/dom/tempotext.h"
#include "internal/palettecell.h"

using namespace mu::palette;
using namespace mu::engraving;

RepriseExpressionCatalogModel::RepriseExpressionCatalogModel(QObject* parent)
    : QAbstractListModel(parent), muse::Contextable(muse::iocCtxForQmlObject(this))
{
}

void RepriseExpressionCatalogModel::classBegin()
{
    auto* provider = dynamic_cast<PaletteProvider*>(m_paletteProvider().get());
    if (!provider) {
        return;
    }
    auto* catalog = qobject_cast<PaletteTreeModel*>(provider->catalogPaletteModel());
    if (!catalog) {
        return;
    }
    m_provider = provider;
    m_source = catalog;
    connect(provider, &QObject::destroyed, this, &RepriseExpressionCatalogModel::invalidate);
    connect(catalog, &QObject::destroyed, this, &RepriseExpressionCatalogModel::invalidate);
    rebuild();
}

int RepriseExpressionCatalogModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : static_cast<int>(m_entries.size());
}

QVariant RepriseExpressionCatalogModel::data(const QModelIndex& index, int role) const
{
    if (!m_source || !index.isValid() || index.parent().isValid()
        || index.column() != 0 || index.row() < 0 || index.row() >= rowCount()) {
        return {};
    }
    const QPersistentModelIndex& nativeIndex = m_entries.at(static_cast<size_t>(index.row()));
    return nativeIndex.isValid() ? m_source->data(nativeIndex, role) : QVariant();
}

QHash<int, QByteArray> RepriseExpressionCatalogModel::roleNames() const
{
    return m_source ? m_source->roleNames() : QAbstractListModel::roleNames();
}

void RepriseExpressionCatalogModel::setCategory(Category category)
{
    if (m_category == category) {
        return;
    }

    m_category = category;
    if (!m_repeatMode) {
        rebuild();
    }
    emit categoryChanged();
}

void RepriseExpressionCatalogModel::setRepeatMode(bool repeatMode)
{
    if (m_repeatMode == repeatMode) {
        return;
    }

    m_repeatMode = repeatMode;
    rebuild();
    emit repeatModeChanged();
}

bool RepriseExpressionCatalogModel::available() const
{
    return m_provider && m_source && !m_entries.empty();
}

int RepriseExpressionCatalogModel::count() const
{
    return rowCount();
}

bool RepriseExpressionCatalogModel::apply(int row)
{
    if (!m_provider || row < 0 || row >= rowCount()) {
        return false;
    }
    const QPersistentModelIndex& nativeIndex = m_entries.at(static_cast<size_t>(row));
    AbstractPaletteController* controller = m_provider->catalogPaletteController();
    return nativeIndex.isValid() && controller
           && controller->applyPaletteElement(nativeIndex, Qt::NoModifier);
}

void RepriseExpressionCatalogModel::appendNativeType(const char* catalogId, ElementType type, Category category)
{
    if (!m_provider || !m_source || (category != All && m_category != All && m_category != category)) {
        return;
    }
    const QModelIndex catalogIndex = m_provider->catalogPaletteIndex(QString::fromLatin1(catalogId));
    if (!catalogIndex.isValid() || catalogIndex.model() != m_source) {
        return;
    }
    for (int row = 0; row < m_source->rowCount(catalogIndex); ++row) {
        const QModelIndex cellIndex = m_source->index(row, 0, catalogIndex);
        const PaletteCellConstPtr cell = m_source->findCell(cellIndex);
        if (!cell || !cell->element || cell->element->type() != type) {
            continue;
        }
        if (type == ElementType::TEMPO_TEXT) {
            const auto* tempo = static_cast<const TempoText*>(cell->element.get());
            const bool isTempoMark = tempo->isNormal() && !tempo->isRelative();
            if (isTempoMark != (category == TempoMarks)) {
                continue;
            }
        }
        m_entries.emplace_back(cellIndex);
    }
}

void RepriseExpressionCatalogModel::rebuild()
{
    beginResetModel();
    m_entries.clear();

    if (m_repeatMode) {
        appendNativeType("repeats-jumps", ElementType::BAR_LINE);
        appendNativeType("repeats-jumps", ElementType::VOLTA);
        appendNativeType("repeats-jumps", ElementType::JUMP);
        appendNativeType("repeats-jumps", ElementType::MARKER);
        appendNativeType("repeats-jumps", ElementType::MEASURE_REPEAT);
    } else {
        appendNativeType("dynamics", ElementType::DYNAMIC, Dynamics);
        appendNativeType("tempo", ElementType::TEMPO_TEXT, TempoMarks);
        appendNativeType("tempo", ElementType::TEMPO_TEXT, TempoChanges);
        appendNativeType("tempo", ElementType::GRADUAL_TEMPO_CHANGE, TempoChanges);
        appendNativeType("text", ElementType::EXPRESSION, ExpressiveText);
        appendNativeType("text", ElementType::PLAYTECH_ANNOTATION, TechniqueText);
        appendNativeType("text", ElementType::REHEARSAL_MARK, RehearsalMarks);
    }

    endResetModel();
    emit catalogChanged();
}

void RepriseExpressionCatalogModel::invalidate()
{
    beginResetModel();
    m_entries.clear();
    m_provider.clear();
    m_source.clear();
    endResetModel();
    emit catalogChanged();
}
