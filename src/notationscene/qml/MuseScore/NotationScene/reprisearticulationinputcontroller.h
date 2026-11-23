/*
 * SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Reprise Notation contributors
 */
#pragma once

#include <memory>

#include <QMetaObject>
#include <QObject>
#include <QPointer>
#include <QPointF>
#include <qqmlintegration.h>

class QEvent;
class QMouseEvent;

namespace mu::engraving {
class Note;
}

namespace mu::notation {
class AbstractNotationPaintView;
class INotationInteraction;
class RepriseToolModel;

// Thin Finale-like input adapter. It never creates engraving objects: it only
// preserves/selects the target MuseScore notes and asks QML to open the native
// Articulations catalog. The catalog controller performs the actual edit/undo.
class RepriseArticulationInputController : public QObject
{
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(QObject* view READ view WRITE setView NOTIFY viewChanged)
    Q_PROPERTY(mu::notation::RepriseToolModel* toolModel READ toolModel WRITE setToolModel NOTIFY toolModelChanged)

public:
    explicit RepriseArticulationInputController(QObject* parent = nullptr);
    ~RepriseArticulationInputController() override;

    QObject* view() const;
    void setView(QObject* view);

    RepriseToolModel* toolModel() const;
    void setToolModel(RepriseToolModel* model);

signals:
    void viewChanged();
    void toolModelChanged();
    void articulationRequested();

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    bool inputAvailable() const;
    mu::engraving::Note* noteAt(const QPointF& viewPosition) const;
    bool selectionContains(const mu::engraving::Note* note) const;
    void ensureSelected(mu::engraving::Note* note);
    bool handleMousePress(QMouseEvent* event);
    bool handleMouseMove(QMouseEvent* event);
    bool handleMouseRelease(QMouseEvent* event);
    void resetPointerOwnership();

    QPointer<AbstractNotationPaintView> m_view;
    QPointer<RepriseToolModel> m_toolModel;
    QMetaObject::Connection m_toolStateConnection;
    QPointF m_pressPosition;
    bool m_ownedPress = false;
    bool m_doubleClick = false;
    std::weak_ptr<INotationInteraction> m_lassoInteraction;
};
}
