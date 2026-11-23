/*
 * SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Reprise Notation contributors
 */
#pragma once

#include <QMetaObject>
#include <QObject>
#include <QPointer>
#include <QPointF>
#include <qqmlintegration.h>

#include "engraving/types/types.h"

class QEvent;
class QMouseEvent;

namespace mu::engraving {
class Measure;
}

namespace mu::notation {
class AbstractNotationPaintView;
class RepriseToolModel;

// Finale-like Clef input adapter. It only resolves/selects the clicked
// MuseScore measure/staff and asks QML to show the clef selection dialog.
// Actual clef application stays in MuseScore's palette/notation pipeline.
class RepriseClefInputController : public QObject
{
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(QObject* view READ view WRITE setView NOTIFY viewChanged)
    Q_PROPERTY(mu::notation::RepriseToolModel* toolModel READ toolModel WRITE setToolModel NOTIFY toolModelChanged)

public:
    explicit RepriseClefInputController(QObject* parent = nullptr);
    ~RepriseClefInputController() override;

    QObject* view() const;
    void setView(QObject* view);

    RepriseToolModel* toolModel() const;
    void setToolModel(RepriseToolModel* model);

signals:
    void viewChanged();
    void toolModelChanged();
    void clefRequested();

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    bool inputAvailable() const;
    mu::engraving::Measure* measureAt(const QPointF& viewPosition, mu::engraving::staff_idx_t* staffIdx) const;
    void selectMeasure(mu::engraving::Measure* measure, mu::engraving::staff_idx_t staffIdx);
    bool handleMousePress(QMouseEvent* event);
    bool handleMouseDoubleClick(QMouseEvent* event);
    bool handleMouseMove(QMouseEvent* event);
    bool handleMouseRelease(QMouseEvent* event);
    void resetPointerOwnership();

    QPointer<AbstractNotationPaintView> m_view;
    QPointer<RepriseToolModel> m_toolModel;
    QMetaObject::Connection m_toolStateConnection;
    bool m_ownedPress = false;
    bool m_requestOnRelease = false;
};
}
