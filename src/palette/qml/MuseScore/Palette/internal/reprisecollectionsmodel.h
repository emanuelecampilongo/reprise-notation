/*
 * SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Reprise Notation contributors
 */
#pragma once

#include <QObject>
#include <QPointer>
#include <QQmlParserStatus>
#include <qqmlintegration.h>

#include "modularity/ioc.h"
#include "internal/paletteprovider.h"

class QSortFilterProxyModel;

namespace mu::palette {
class RepriseCollectionsModel : public QObject, public QQmlParserStatus, public muse::Contextable
{
    Q_OBJECT
    Q_INTERFACES(QQmlParserStatus)
    QML_ELEMENT

    Q_PROPERTY(mu::palette::PaletteProvider* provider READ provider NOTIFY modelsChanged)
    Q_PROPERTY(QAbstractItemModel* paletteModel READ paletteModel CONSTANT)
    Q_PROPERTY(mu::palette::AbstractPaletteController* paletteController READ paletteController NOTIFY modelsChanged)
    Q_PROPERTY(bool available READ available NOTIFY modelsChanged)

    muse::ContextInject<IPaletteProvider> m_paletteProvider = { this };

public:
    explicit RepriseCollectionsModel(QObject* parent = nullptr);

    void classBegin() override;
    void componentComplete() override {}

    PaletteProvider* provider() const;
    QAbstractItemModel* paletteModel() const;
    AbstractPaletteController* paletteController() const;
    bool available() const;

signals:
    void modelsChanged();

private:
    void invalidate();

    QPointer<PaletteProvider> m_provider;
    QPointer<PaletteTreeModel> m_source;
    QPointer<UserPaletteController> m_controller;
    QSortFilterProxyModel* m_visibilityFilter = nullptr;
    bool m_initialized = false;
};
}
