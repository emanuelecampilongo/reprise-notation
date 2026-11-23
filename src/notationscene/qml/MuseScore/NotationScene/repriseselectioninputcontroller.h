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

class QEvent;
class QMouseEvent;

namespace mu::notation {
class AbstractNotationPaintView;
class RepriseToolModel;

// Small Reprise-only adapter for Finale-like Selection behavior. Native
// MuseScore selection remains untouched; this controller only consumes the
// second click of an empty-staff double click so it cannot enter note input.
class RepriseSelectionInputController : public QObject
{
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(QObject* view READ view WRITE setView NOTIFY viewChanged)
    Q_PROPERTY(mu::notation::RepriseToolModel* toolModel READ toolModel WRITE setToolModel NOTIFY toolModelChanged)

public:
    explicit RepriseSelectionInputController(QObject* parent = nullptr);
    ~RepriseSelectionInputController() override;

    QObject* view() const;
    void setView(QObject* view);

    RepriseToolModel* toolModel() const;
    void setToolModel(RepriseToolModel* model);

signals:
    void viewChanged();
    void toolModelChanged();

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    bool inputAvailable() const;
    bool isEmptyStaffArea(const QPointF& viewPosition) const;
    bool handleDoubleClick(QMouseEvent* event);
    bool handleRelease(QMouseEvent* event);
    void reset();

    QPointer<AbstractNotationPaintView> m_view;
    QPointer<RepriseToolModel> m_toolModel;
    QMetaObject::Connection m_toolStateConnection;
    bool m_releaseOwned = false;
};
}
