/*
 * SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Reprise Notation contributors
 */
#include "reprisecollectionsmodel.h"

#include <QSortFilterProxyModel>

using namespace mu::palette;

RepriseCollectionsModel::RepriseCollectionsModel(QObject* parent)
    : QObject(parent), muse::Contextable(muse::iocCtxForQmlObject(this)),
    m_visibilityFilter(new QSortFilterProxyModel(this))
{
    m_visibilityFilter->setFilterRole(PaletteTreeModel::VisibleRole);
    m_visibilityFilter->setFilterFixedString("true");
}

void RepriseCollectionsModel::classBegin()
{
    if (m_initialized) {
        return;
    }
    m_initialized = true;

    auto* paletteProvider = dynamic_cast<PaletteProvider*>(m_paletteProvider().get());
    if (!paletteProvider || !paletteProvider->userPaletteModel()) {
        return;
    }

    m_provider = paletteProvider;
    m_source = paletteProvider->userPaletteModel();
    m_visibilityFilter->setSourceModel(m_source.data());
    m_controller = new UserPaletteController(m_visibilityFilter, m_source.data(), iocContext(), this);

    connect(paletteProvider, &QObject::destroyed, this, &RepriseCollectionsModel::invalidate);
    connect(m_source.data(), &QObject::destroyed, this, &RepriseCollectionsModel::invalidate);
    emit modelsChanged();
}

PaletteProvider* RepriseCollectionsModel::provider() const
{
    return m_provider.data();
}

QAbstractItemModel* RepriseCollectionsModel::paletteModel() const
{
    return m_visibilityFilter;
}

AbstractPaletteController* RepriseCollectionsModel::paletteController() const
{
    return m_controller.data();
}

bool RepriseCollectionsModel::available() const
{
    return m_provider && m_source && m_controller;
}

void RepriseCollectionsModel::invalidate()
{
    m_visibilityFilter->setSourceModel(nullptr);
    m_provider.clear();
    m_source.clear();
    if (m_controller) {
        m_controller->deleteLater();
        m_controller.clear();
    }
    emit modelsChanged();
}
