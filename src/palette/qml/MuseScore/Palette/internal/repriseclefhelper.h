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

// Presents MuseScore's master Clefs catalog as a Reprise selection list.
// Every entry remains a native palette cell and application delegates to
// MuseScore's catalog palette controller.
class RepriseClefHelper : public QAbstractListModel, public QQmlParserStatus, public muse::Contextable
{
    Q_OBJECT
    Q_INTERFACES(QQmlParserStatus)
    QML_ELEMENT

    Q_PROPERTY(bool available READ available NOTIFY modelChanged)
    Q_PROPERTY(int count READ count NOTIFY modelChanged)

    muse::ContextInject<IPaletteProvider> m_paletteProvider = { this };

public:
    explicit RepriseClefHelper(QObject* parent = nullptr);

    void classBegin() override;
    void componentComplete() override {}

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    bool available() const;
    int count() const;

    Q_INVOKABLE bool apply(int row);

signals:
    void modelChanged();

private:
    QModelIndex catalogPaletteIndex() const;
    void rebuild();
    void invalidate();

    QPointer<PaletteProvider> m_provider;
    QPointer<PaletteTreeModel> m_catalogModel;
    std::vector<QPersistentModelIndex> m_entries;
};
}
