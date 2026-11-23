/*
 * SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Reprise Notation contributors
 */
#include "reprisetimesignaturehelper.h"

#include "internal/palette.h"
#include "internal/palettecell.h"

using namespace mu::palette;

RepriseTimeSignatureHelper::RepriseTimeSignatureHelper(QObject* parent)
    : QAbstractListModel(parent), muse::Contextable(muse::iocCtxForQmlObject(this))
{
}

void RepriseTimeSignatureHelper::classBegin()
{
    auto* provider = dynamic_cast<PaletteProvider*>(m_paletteProvider().get());
    if (!provider || !provider->userPaletteModel()) {
        return;
    }

    auto* catalogModel = qobject_cast<PaletteTreeModel*>(provider->catalogPaletteModel());
    if (!catalogModel) {
        return;
    }

    m_provider = provider;
    m_catalogModel = catalogModel;
    m_userModel = provider->userPaletteModel();

    // Application of an already-created custom cell only needs MuseScore's
    // ordinary palette apply path. Creation itself uses the native custom
    // elements controller below, exactly as MoreElementsPopup does.
    m_applyController = new UserPaletteController(m_userModel.data(), m_userModel.data(), iocContext(), this);

    connect(provider, &QObject::destroyed, this, &RepriseTimeSignatureHelper::invalidate);
    connect(m_catalogModel.data(), &QObject::destroyed, this, &RepriseTimeSignatureHelper::invalidate);
    connect(m_userModel.data(), &QObject::destroyed, this, &RepriseTimeSignatureHelper::invalidate);
    connect(m_userModel.data(), &PaletteTreeModel::treeChanged, this, &RepriseTimeSignatureHelper::rebuild);

    rebuild();
}

int RepriseTimeSignatureHelper::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : static_cast<int>(m_entries.size());
}

QVariant RepriseTimeSignatureHelper::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.parent().isValid() || index.column() != 0
        || index.row() < 0 || index.row() >= rowCount()) {
        return {};
    }

    const QPersistentModelIndex& sourceIndex = m_entries.at(static_cast<size_t>(index.row())).index;
    return sourceIndex.isValid() && sourceIndex.model()
           ? sourceIndex.model()->data(sourceIndex, role) : QVariant();
}

QHash<int, QByteArray> RepriseTimeSignatureHelper::roleNames() const
{
    return m_catalogModel ? m_catalogModel->roleNames() : QAbstractListModel::roleNames();
}

bool RepriseTimeSignatureHelper::available() const
{
    return m_provider && m_catalogModel && m_userModel && !m_entries.empty();
}

bool RepriseTimeSignatureHelper::creatorAvailable() const
{
    return customElementsPaletteIndex().isValid() && customElementsController();
}

int RepriseTimeSignatureHelper::count() const
{
    return rowCount();
}

bool RepriseTimeSignatureHelper::apply(int row)
{
    if (!m_provider || row < 0 || row >= rowCount()) {
        return false;
    }

    const Entry& entry = m_entries.at(static_cast<size_t>(row));
    if (!entry.index.isValid()) {
        return false;
    }

    AbstractPaletteController* controller = entry.catalog
                                            ? m_provider->catalogPaletteController()
                                            : m_applyController.data();
    return controller && controller->applyPaletteElement(entry.index, Qt::NoModifier);
}

bool RepriseTimeSignatureHelper::openCreator()
{
    const QModelIndex paletteIndex = customElementsPaletteIndex();
    AbstractPaletteController* controller = customElementsController();
    if (!paletteIndex.isValid() || !controller) {
        return false;
    }

    PaletteElementEditor* editor = controller->elementEditor(paletteIndex);
    if (!editor || !editor->valid()) {
        return false;
    }

    editor->open();
    return true;
}

QModelIndex RepriseTimeSignatureHelper::catalogPaletteIndex() const
{
    if (!m_provider || !m_catalogModel) {
        return {};
    }

    const QModelIndex index = m_provider->catalogPaletteIndex(QStringLiteral("time-signatures"));
    return index.isValid() && index.model() == m_catalogModel ? index : QModelIndex();
}

QModelIndex RepriseTimeSignatureHelper::userPaletteIndex() const
{
    if (!m_userModel) {
        return {};
    }

    QModelIndex found;
    for (int row = 0; row < m_userModel->rowCount(); ++row) {
        const QModelIndex index = m_userModel->index(row, 0);
        const auto type = index.data(PaletteTreeModel::PaletteContentTypeRole).value<Palette::Type>();
        if (type != Palette::Type::TimeSig) {
            continue;
        }
        if (found.isValid()) {
            return {};
        }
        found = index;
    }
    return found;
}

QModelIndex RepriseTimeSignatureHelper::customElementsPaletteIndex() const
{
    if (!m_provider) {
        return {};
    }
    const QModelIndex userIndex = userPaletteIndex();
    return userIndex.isValid() ? m_provider->customElementsPaletteIndex(userIndex) : QModelIndex();
}

AbstractPaletteController* RepriseTimeSignatureHelper::customElementsController() const
{
    if (!m_provider) {
        return nullptr;
    }

    // MuseScore intentionally exposes this controller to QML as a Q_PROPERTY
    // while keeping the C++ getter private. Respect that boundary instead of
    // widening PaletteProvider's upstream API just for Reprise.
    return m_provider->property("customElementsPaletteController").value<AbstractPaletteController*>();
}

void RepriseTimeSignatureHelper::rebuild()
{
    beginResetModel();
    m_entries.clear();

    if (m_catalogModel) {
        const QModelIndex catalogRoot = catalogPaletteIndex();
        if (catalogRoot.isValid()) {
            const int rows = m_catalogModel->rowCount(catalogRoot);
            for (int row = 0; row < rows; ++row) {
                const QModelIndex index = m_catalogModel->index(row, 0, catalogRoot);
                if (m_catalogModel->findCell(index)) {
                    m_entries.push_back({ QPersistentModelIndex(index), true });
                }
            }
        }
    }

    if (m_userModel) {
        const QModelIndex userRoot = userPaletteIndex();
        const QModelIndex masterRoot = catalogPaletteIndex();
        const Palette* masterPalette = m_catalogModel && masterRoot.isValid()
                                       ? m_catalogModel->findPalette(masterRoot) : nullptr;

        if (userRoot.isValid()) {
            const int rows = m_userModel->rowCount(userRoot);
            for (int row = 0; row < rows; ++row) {
                const QModelIndex index = m_userModel->index(row, 0, userRoot);
                const PaletteCellPtr cell = m_userModel->findCell(index);
                if (!cell || !cell->element) {
                    continue;
                }
                if (masterPalette && masterPalette->indexOfCell(*cell, false) >= 0) {
                    continue;
                }
                m_entries.push_back({ QPersistentModelIndex(index), false });
            }
        }
    }

    endResetModel();
    emit modelChanged();
}

void RepriseTimeSignatureHelper::invalidate()
{
    beginResetModel();
    m_entries.clear();
    m_provider.clear();
    m_catalogModel.clear();
    m_userModel.clear();
    if (m_applyController) {
        m_applyController->deleteLater();
        m_applyController.clear();
    }
    endResetModel();
    emit modelChanged();
}
