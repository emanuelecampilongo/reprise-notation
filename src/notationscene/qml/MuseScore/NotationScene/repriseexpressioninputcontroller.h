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

namespace mu::engraving {
class EngravingItem;
}

namespace mu::notation {
class AbstractNotationPaintView;
class RepriseToolModel;

// Thin Finale-like input adapter. It never creates engraving objects: it only
// preserves/selects the target MuseScore notes or rests and asks QML to open the native
// Expressions catalog. The catalog controller performs the actual edit/undo.
class RepriseExpressionInputController : public QObject
{
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(QObject* view READ view WRITE setView NOTIFY viewChanged)
    Q_PROPERTY(mu::notation::RepriseToolModel* toolModel READ toolModel WRITE setToolModel NOTIFY toolModelChanged)

public:
    explicit RepriseExpressionInputController(QObject* parent = nullptr);
    ~RepriseExpressionInputController() override;

    QObject* view() const;
    void setView(QObject* view);

    RepriseToolModel* toolModel() const;
    void setToolModel(RepriseToolModel* model);

signals:
    void viewChanged();
    void toolModelChanged();
    void expressionRequested();

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    bool inputAvailable() const;
    mu::engraving::EngravingItem* targetAt(const QPointF& viewPosition) const;
    bool selectionContains(const mu::engraving::EngravingItem* target) const;
    void ensureSelected(mu::engraving::EngravingItem* target);
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
