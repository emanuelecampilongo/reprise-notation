/*
 * SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Reprise Notation contributors
 */
#include "reprisearticulationcatalogmodel.h"

#include "internal/palettecell.h"
#include "internal/palettemodel.h"

#include "engraving/dom/actionicon.h"
#include "engraving/dom/arpeggio.h"
#include "engraving/dom/articulation.h"
#include "engraving/dom/breath.h"
#include "engraving/dom/fermata.h"
#include "engraving/dom/fingering.h"
#include "engraving/dom/ornament.h"
#include "engraving/dom/tremolosinglechord.h"
#include "engraving/types/types.h"

using namespace mu::engraving;
using namespace mu::palette;

namespace {
bool isFinaleNativeArticulation(const EngravingItem* item)
{
    // MuseScore's Articulations palette also contains a Slur and several
    // technique-specific symbols. Reprise exposes the conventional one-note
    // articulation family used by Finale's Articulation tool.
    if (!item || item->type() != ElementType::ARTICULATION) {
        return false;
    }

    switch (static_cast<const Articulation*>(item)->symId()) {
    case SymId::articAccentAbove:
    case SymId::articStaccatoAbove:
    case SymId::articStaccatissimoAbove:
    case SymId::articTenutoAbove:
    case SymId::articTenutoStaccatoAbove:
    case SymId::articMarcatoAbove:
    case SymId::articAccentStaccatoAbove:
    case SymId::articMarcatoStaccatoAbove:
    case SymId::articMarcatoTenutoAbove:
    case SymId::articStaccatissimoStrokeAbove:
    case SymId::articStaccatissimoWedgeAbove:
    case SymId::articStressAbove:
    case SymId::articTenutoAccentAbove:
    case SymId::articUnstressAbove:
    case SymId::stringsHarmonic:
    case SymId::stringsUpBow:
    case SymId::stringsDownBow:
        return true;
    default:
        return false;
    }
}

bool isFinaleOrnament(const EngravingItem* item)
{
    if (!item || item->type() != ElementType::ORNAMENT) {
        return false;
    }

    switch (static_cast<const Ornament*>(item)->symId()) {
    case SymId::ornamentTrill:
    case SymId::ornamentShortTrill:
    case SymId::ornamentMordent:
    case SymId::ornamentTurn:
    case SymId::ornamentTurnInverted:
        return true;
    default:
        return false;
    }
}

bool isFinaleFermata(const EngravingItem* item)
{
    return item && item->type() == ElementType::FERMATA
           && static_cast<const Fermata*>(item)->symId() == SymId::fermataAbove;
}

bool isFinaleSingleNoteTremolo(const EngravingItem* item)
{
    if (!item || item->type() != ElementType::TREMOLO_SINGLECHORD) {
        return false;
    }

    switch (static_cast<const TremoloSingleChord*>(item)->tremoloType()) {
    case TremoloType::R8:
    case TremoloType::R16:
    case TremoloType::R32:
        return true;
    default:
        return false;
    }
}

bool isFinaleBreathOrCaesura(const EngravingItem* item)
{
    if (!item || item->type() != ElementType::BREATH) {
        return false;
    }

    switch (static_cast<const Breath*>(item)->symId()) {
    case SymId::breathMarkComma:
    case SymId::caesura:
        return true;
    default:
        return false;
    }
}

bool isFinaleFingeringNumber(const EngravingItem* item)
{
    if (!item || item->type() != ElementType::FINGERING) {
        return false;
    }

    const QString text = static_cast<const Fingering*>(item)->plainText().toQString();
    return text.size() == 1 && text.at(0) >= QLatin1Char('1') && text.at(0) <= QLatin1Char('5');
}

bool isRolledChord(const EngravingItem* item)
{
    return item && item->type() == ElementType::ARPEGGIO
           && static_cast<const Arpeggio*>(item)->arpeggioType() == ArpeggioType::NORMAL;
}

bool isSlashedGraceNote(const EngravingItem* item)
{
    return item && item->type() == ElementType::ACTION_ICON
           && static_cast<const ActionIcon*>(item)->actionType() == ActionIconType::ACCIACCATURA;
}

bool isParenthesesAction(const EngravingItem* item)
{
    return item && item->type() == ElementType::ACTION_ICON
           && static_cast<const ActionIcon*>(item)->actionType() == ActionIconType::PARENTHESES;
}
}

RepriseArticulationCatalogModel::RepriseArticulationCatalogModel(QObject* parent)
    : QAbstractListModel(parent), muse::Contextable(muse::iocCtxForQmlObject(this))
{
}

void RepriseArticulationCatalogModel::classBegin()
{
    auto* provider = dynamic_cast<PaletteProvider*>(m_paletteProvider().get());
    if (!provider) {
        return;
    }

    auto* source = qobject_cast<PaletteTreeModel*>(provider->catalogPaletteModel());
    if (!source) {
        return;
    }

    m_provider = provider;
    m_source = source;

    connect(provider, &QObject::destroyed, this, &RepriseArticulationCatalogModel::invalidate);
    connect(source, &QObject::destroyed, this, &RepriseArticulationCatalogModel::invalidate);

    rebuild();
}

int RepriseArticulationCatalogModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : static_cast<int>(m_entries.size());
}

QVariant RepriseArticulationCatalogModel::data(const QModelIndex& index, int role) const
{
    if (!m_source || !index.isValid() || index.parent().isValid()
        || index.column() != 0 || index.row() < 0 || index.row() >= rowCount()) {
        return {};
    }

    const QPersistentModelIndex& sourceIndex = m_entries.at(static_cast<size_t>(index.row()));
    return sourceIndex.isValid() ? m_source->data(sourceIndex, role) : QVariant();
}

QHash<int, QByteArray> RepriseArticulationCatalogModel::roleNames() const
{
    return m_source ? m_source->roleNames() : QAbstractListModel::roleNames();
}

bool RepriseArticulationCatalogModel::available() const
{
    return m_provider && m_source && !m_entries.empty();
}

int RepriseArticulationCatalogModel::count() const
{
    return rowCount();
}

bool RepriseArticulationCatalogModel::apply(int row)
{
    if (!m_provider || row < 0 || row >= rowCount()) {
        return false;
    }

    const QPersistentModelIndex& sourceIndex = m_entries.at(static_cast<size_t>(row));
    AbstractPaletteController* controller = m_provider->catalogPaletteController();
    return sourceIndex.isValid() && controller
           && controller->applyPaletteElement(sourceIndex, Qt::NoModifier);
}

void RepriseArticulationCatalogModel::rebuild()
{
    beginResetModel();
    m_entries.clear();

    // Finale's Articulation Selection is a workflow category rather than a
    // one-to-one copy of MuseScore's Articulations palette. Keep the real
    // MuseScore cells, but collect the native families used by that workflow.
    appendMatching("articulations", isFinaleNativeArticulation);
    appendMatching("ornaments", isFinaleOrnament);
    appendMatching("breaths-pauses", isFinaleFermata);
    appendMatching("tremolos", isFinaleSingleNoteTremolo);
    appendMatching("breaths-pauses", isFinaleBreathOrCaesura);
    appendMatching("fingerings", isFinaleFingeringNumber);
    appendFirstMatching("arpeggios-glissandos", isRolledChord);
    appendFirstMatching("grace-notes", isSlashedGraceNote);
    appendFirstMatching("accidentals", isParenthesesAction);

    endResetModel();
    emit catalogChanged();
}

void RepriseArticulationCatalogModel::invalidate()
{
    beginResetModel();
    m_entries.clear();
    m_provider.clear();
    m_source.clear();
    endResetModel();
    emit catalogChanged();
}

void RepriseArticulationCatalogModel::appendMatching(const char* catalogId, CellPredicate predicate)
{
    if (!m_provider || !m_source || !catalogId || !predicate) {
        return;
    }

    const QModelIndex parent = m_provider->catalogPaletteIndex(QString::fromLatin1(catalogId));
    if (!parent.isValid()) {
        return;
    }

    const int rows = m_source->rowCount(parent);
    for (int row = 0; row < rows; ++row) {
        const QModelIndex cellIndex = m_source->index(row, 0, parent);
        const PaletteCell* cell = m_source->data(cellIndex, PaletteTreeModel::PaletteCellRole).value<const PaletteCell*>();
        if (cell && cell->element && predicate(cell->element.get())) {
            m_entries.emplace_back(cellIndex);
        }
    }
}

void RepriseArticulationCatalogModel::appendFirstMatching(const char* catalogId, CellPredicate predicate)
{
    if (!m_provider || !m_source || !catalogId || !predicate) {
        return;
    }

    const QModelIndex parent = m_provider->catalogPaletteIndex(QString::fromLatin1(catalogId));
    if (!parent.isValid()) {
        return;
    }

    const int rows = m_source->rowCount(parent);
    for (int row = 0; row < rows; ++row) {
        const QModelIndex cellIndex = m_source->index(row, 0, parent);
        const PaletteCell* cell = m_source->data(cellIndex, PaletteTreeModel::PaletteCellRole).value<const PaletteCell*>();
        if (cell && cell->element && predicate(cell->element.get())) {
            m_entries.emplace_back(cellIndex);
            return;
        }
    }
}
