/*
 * SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Reprise Notation contributors
 */
#include "repriseselectioninputcontroller.h"

#include <algorithm>

#include <QEvent>
#include <QMouseEvent>

#include "abstractnotationpaintview.h"
#include "reprisetoolmodel.h"

#include "engraving/dom/engravingitem.h"
#include "engraving/dom/rest.h"
#include "notation/inotationinteraction.h"

using namespace mu;
using namespace mu::notation;
using namespace mu::engraving;

namespace {
constexpr qreal EMPTY_STAFF_HIT_RADIUS_PX = 2.0;
}

RepriseSelectionInputController::RepriseSelectionInputController(QObject* parent)
    : QObject(parent)
{
}

RepriseSelectionInputController::~RepriseSelectionInputController()
{
    if (m_view) {
        m_view->removeEventFilter(this);
    }
}

QObject* RepriseSelectionInputController::view() const
{
    return m_view.data();
}

void RepriseSelectionInputController::setView(QObject* view)
{
    auto* notationView = qobject_cast<AbstractNotationPaintView*>(view);
    if (m_view == notationView) {
        return;
    }

    if (m_view) {
        m_view->removeEventFilter(this);
    }
    reset();
    m_view = notationView;
    if (m_view) {
        m_view->installEventFilter(this);
    }
    emit viewChanged();
}

RepriseToolModel* RepriseSelectionInputController::toolModel() const
{
    return m_toolModel.data();
}

void RepriseSelectionInputController::setToolModel(RepriseToolModel* model)
{
    if (m_toolModel == model) {
        return;
    }

    QObject::disconnect(m_toolStateConnection);
    reset();
    m_toolModel = model;
    if (m_toolModel) {
        m_toolStateConnection = connect(m_toolModel, &RepriseToolModel::stateChanged, this, [this]() {
            if (!m_toolModel || m_toolModel->activeTool() != RepriseToolModel::Selection) {
                reset();
            }
        });
    }
    emit toolModelChanged();
}

bool RepriseSelectionInputController::inputAvailable() const
{
    if (!m_view || !m_toolModel || !m_toolModel->enabled()
        || m_toolModel->activeTool() != RepriseToolModel::Selection
        || !m_view->isMainView() || m_view->publishMode() || !m_view->hasActiveFocus()
        || m_view->isNoteEnterMode()) {
        return false;
    }

    const INotationInteractionPtr interaction = m_view->notationInteraction();
    return interaction
           && !interaction->isTextEditingStarted()
           && !interaction->isGripEditStarted()
           && !interaction->isEditingElement()
           && !interaction->isDragStarted()
           && !interaction->isOutgoingDragStarted();
}

bool RepriseSelectionInputController::isEmptyStaffArea(const QPointF& viewPosition) const
{
    if (!m_view) {
        return false;
    }

    const INotationInteractionPtr interaction = m_view->notationInteraction();
    if (!interaction) {
        return false;
    }

    const PointF logicalPosition = m_view->toLogical(viewPosition);
    if (!interaction->hitStaff(logicalPosition)) {
        return false;
    }

    const qreal scale = std::max(m_view->currentScaling(), 0.001);
    const float hitWidth = static_cast<float>(EMPTY_STAFF_HIT_RADIUS_PX / scale);
    const std::vector<EngravingItem*> hitItems = interaction->hitElements(logicalPosition, hitWidth);

    bool hasEmptyStaffFallback = false;
    for (EngravingItem* item : hitItems) {
        if (!item) {
            continue;
        }
        if (item->isMeasure() || item->isStaffLines() || item->isMMRest()
            || (item->isRest() && toRest(item)->isFullMeasureRest())) {
            hasEmptyStaffFallback = true;
            continue;
        }

        // Preserve MuseScore's native double-click semantics for every real
        // notation object. Reprise only changes empty-measure/staff hits.
        return false;
    }

    return hasEmptyStaffFallback;
}

bool RepriseSelectionInputController::handleDoubleClick(QMouseEvent* event)
{
    if (event->button() != Qt::LeftButton || event->modifiers() != Qt::NoModifier
        || !isEmptyStaffArea(event->position())) {
        return false;
    }

    // Qt has already delivered the first click, so MuseScore can perform its
    // normal selection. Consume only the second click and its release; this is
    // the part that would otherwise switch an empty measure into note input.
    m_releaseOwned = true;
    event->accept();
    return true;
}

bool RepriseSelectionInputController::handleRelease(QMouseEvent* event)
{
    if (!m_releaseOwned || event->button() != Qt::LeftButton) {
        return false;
    }

    reset();
    event->accept();
    return true;
}

void RepriseSelectionInputController::reset()
{
    m_releaseOwned = false;
}

bool RepriseSelectionInputController::eventFilter(QObject* watched, QEvent* event)
{
    if (watched != m_view) {
        return QObject::eventFilter(watched, event);
    }

    if (!inputAvailable()) {
        reset();
        return QObject::eventFilter(watched, event);
    }

    switch (event->type()) {
    case QEvent::MouseButtonDblClick:
        return handleDoubleClick(static_cast<QMouseEvent*>(event));
    case QEvent::MouseButtonRelease:
        if (handleRelease(static_cast<QMouseEvent*>(event))) {
            return true;
        }
        break;
    case QEvent::FocusOut:
    case QEvent::UngrabMouse:
        reset();
        break;
    default:
        break;
    }

    return QObject::eventFilter(watched, event);
}
