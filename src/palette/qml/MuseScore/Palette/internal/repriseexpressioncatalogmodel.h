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

namespace mu::palette {

// Finale-like Expression Selection with only MuseScore native palette cells.
// No custom engraving elements or replacement application pipeline.
class RepriseExpressionCatalogModel : public QAbstractListModel, public QQmlParserStatus, public muse::Contextable
{
    Q_OBJECT
    Q_INTERFACES(QQmlParserStatus)
    QML_ELEMENT

    Q_PROPERTY(Category category READ category WRITE setCategory NOTIFY categoryChanged)
    Q_PROPERTY(bool repeatMode READ repeatMode WRITE setRepeatMode NOTIFY repeatModeChanged)
    Q_PROPERTY(bool available READ available NOTIFY catalogChanged)
    Q_PROPERTY(int count READ count NOTIFY catalogChanged)

    muse::ContextInject<IPaletteProvider> m_paletteProvider = { this };

public:
    enum Category {
        All,
        Dynamics,
        TempoMarks,
        TempoChanges,
        ExpressiveText,
        TechniqueText,
        RehearsalMarks
    };
    Q_ENUM(Category)

    Category category() const { return m_category; }
    void setCategory(Category category);

    explicit RepriseExpressionCatalogModel(QObject* parent = nullptr);

    void classBegin() override;
    void componentComplete() override {}

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;
    bool repeatMode() const { return m_repeatMode; }
    void setRepeatMode(bool repeatMode);
    bool available() const;
    int count() const;

    Q_INVOKABLE bool apply(int row);

signals:
    void catalogChanged();
    void categoryChanged();
    void repeatModeChanged();

private:
    void rebuild();
    void invalidate();
    void appendNativeType(const char* catalogId, engraving::ElementType type, Category category = All);

    Category m_category = All;
    bool m_repeatMode = false;
    QPointer<PaletteProvider> m_provider;
    QPointer<PaletteTreeModel> m_source;
    std::vector<QPersistentModelIndex> m_entries;
};
}
