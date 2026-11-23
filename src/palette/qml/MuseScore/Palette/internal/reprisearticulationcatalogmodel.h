/*
 * SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Reprise Notation contributors
 */
#pragma once

#include <vector>

#include <QAbstractListModel>
#include <QPersistentModelIndex>
#include <QPointer>
#include <QQmlParserStatus>
#include <qqmlintegration.h>

#include "modularity/ioc.h"
#include "internal/paletteprovider.h"

namespace mu::engraving {
class EngravingItem;
}

namespace mu::palette {
class PaletteTreeModel;

// Presents a Finale-oriented Articulation Selection while keeping every item
// backed by an existing MuseScore master-palette cell. The model never creates
// engraving objects; apply() delegates to MuseScore's catalog controller.
class RepriseArticulationCatalogModel : public QAbstractListModel, public QQmlParserStatus, public muse::Contextable
{
    Q_OBJECT
    Q_INTERFACES(QQmlParserStatus)
    QML_ELEMENT

    Q_PROPERTY(bool available READ available NOTIFY catalogChanged)
    Q_PROPERTY(int count READ count NOTIFY catalogChanged)

    muse::ContextInject<IPaletteProvider> m_paletteProvider = { this };

public:
    explicit RepriseArticulationCatalogModel(QObject* parent = nullptr);

    void classBegin() override;
    void componentComplete() override {}

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    bool available() const;
    int count() const;

    Q_INVOKABLE bool apply(int row);

signals:
    void catalogChanged();

private:
    using CellPredicate = bool (*)(const mu::engraving::EngravingItem*);

    void rebuild();
    void invalidate();
    void appendMatching(const char* catalogId, CellPredicate predicate);
    void appendFirstMatching(const char* catalogId, CellPredicate predicate);

    QPointer<PaletteProvider> m_provider;
    QPointer<PaletteTreeModel> m_source;
    std::vector<QPersistentModelIndex> m_entries;
};
}
