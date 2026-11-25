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

// Presents MuseScore's master Key Signature catalog plus user-created key
// signatures in one Reprise selection list. Every entry remains backed by a
// native palette cell; creation delegates to MuseScore's PaletteElementEditor.
class RepriseKeySignatureHelper : public QAbstractListModel, public QQmlParserStatus, public muse::Contextable
{
    Q_OBJECT
    Q_INTERFACES(QQmlParserStatus)
    QML_ELEMENT

    Q_PROPERTY(bool available READ available NOTIFY modelChanged)
    Q_PROPERTY(bool creatorAvailable READ creatorAvailable NOTIFY modelChanged)
    Q_PROPERTY(int count READ count NOTIFY modelChanged)

    muse::ContextInject<IPaletteProvider> m_paletteProvider = { this };
    muse::ContextInject<context::IGlobalContext> m_globalContext = { this };

public:
    explicit RepriseKeySignatureHelper(QObject* parent = nullptr);

    void classBegin() override;
    void componentComplete() override {}

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    bool available() const;
    bool creatorAvailable() const;
    int count() const;

    Q_INVOKABLE bool apply(int row, bool transposeSelection = false);
    Q_INVOKABLE bool canTransposeSelection(int row) const;
    Q_INVOKABLE bool openCreator();

signals:
    void modelChanged();

private:
    struct Entry {
        QPersistentModelIndex index;
        bool catalog = true;
    };

    QModelIndex catalogPaletteIndex() const;
    QModelIndex userPaletteIndex() const;
    QModelIndex customElementsPaletteIndex() const;
    AbstractPaletteController* customElementsController() const;
    void rebuild();
    void invalidate();

    QPointer<PaletteProvider> m_provider;
    QPointer<PaletteTreeModel> m_catalogModel;
    QPointer<PaletteTreeModel> m_userModel;
    QPointer<UserPaletteController> m_applyController;
    std::vector<Entry> m_entries;
};
}
