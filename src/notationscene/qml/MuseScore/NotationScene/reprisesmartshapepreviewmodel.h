/*
 * SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Reprise Notation contributors
 */
#pragma once

#include <QObject>
#include <QHash>
#include <QVariant>
#include <qqmlintegration.h>

#include "engraving/dom/engravingitem.h"

namespace mu::notation {

// Provides actual native MuseScore engraving objects for the 48x28 Shape
// previews. The existing PaintedEngravingItem owns painting and layout.
class RepriseSmartShapePreviewModel : public QObject
{
    Q_OBJECT
    QML_ELEMENT

public:
    explicit RepriseSmartShapePreviewModel(QObject* parent = nullptr);

    Q_INVOKABLE QVariant previewElement(int shape);

private:
    engraving::ElementPtr makePreviewElement(int shape) const;

    QHash<int, engraving::ElementPtr> m_elements;
};
}
