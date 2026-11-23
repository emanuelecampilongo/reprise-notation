/*
 * SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Reprise Notation contributors
 */
#include "repriseexpressioninputcontroller.h"

#include <algorithm>
#include <limits>

#include <QEvent>
#include <QMouseEvent>

#include "abstractnotationpaintview.h"
#include "reprisetoolmodel.h"

#include "engraving/dom/chord.h"
#include "engraving/dom/note.h"
#include "notation/inotationinteraction.h"
#include "notation/inotationselection.h"

using namespace mu;
using namespace mu::notation;
using namespace mu::engraving;

namespace {
constexpr qreal EXPRESSION_NOTE_HIT_RADIUS_PX = 16.0;
}

RepriseExpressionInputController::RepriseExpressionInputController(QObject* parent)
    : QObject(parent)
{
}

RepriseExpressionInputController::~RepriseExpressionInputController()
{
    if (m_view) {
        m_view->removeEventFilter(this);
    }
}

QObject* RepriseExpressionInputController::view() const
{
    return m_view.data();
}

void RepriseExpressionInputController::setView(QObject* view)
{
    auto* notationView = qobject_cast<AbstractNotationPaintView*>(view);
    if (m_view == notationView) {
        return;
    }

    if (m_view) {
        m_view->removeEventFilter(this);
    }
    resetPointerOwnership();
    m_view = notationView;
    if (m_view) {
        m_view->installEventFilter(this);
    }
    emit viewChanged();
}

RepriseToolModel* RepriseExpressionInputController::toolModel() const
{
    return m_toolModel.data();
}

void RepriseExpressionInputController::setToolModel(RepriseToolModel* model)
{
    if (m_toolModel == model) {
        return;
    }

    QObject::disconnect(m_toolStateConnection);
    resetPointerOwnership();
    m_toolModel = model;
    if (m_toolModel) {
        m_toolStateConnection = connect(m_toolModel, &RepriseToolModel::stateChanged, this, [this]() {
            if (!m_toolModel || m_toolModel->activeTool() != RepriseToolModel::Expression) {
                resetPointerOwnership();
            }
        });
    }
    emit toolModelChanged();
}

bool RepriseExpressionInputController::inputAvailable() const
{
    if (!m_view || !m_toolModel || !m_toolModel->enabled()
        || m_toolModel->activeTool() != RepriseToolModel::Expression
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

EngravingItem* RepriseExpressionInputController::targetAt(const QPointF& viewPosition) const
{
    if (!m_view) {
        return nullptr;
    }
    const INotationInteractionPtr interaction = m_view->notationInteraction();
    if (!interaction) {
        return nullptr;
    }

    const qreal scale = std::max(m_view->currentScaling(), 0.001);
    const PointF logicalPosition = m_view->toLogical(viewPosition);
    const float hitWidth = static_cast<float>(EXPRESSION_NOTE_HIT_RADIUS_PX / scale);
    const std::vector<EngravingItem*> hitItems = interaction->hitElements(logicalPosition, hitWidth);

    EngravingItem* closest = nullptr;
    double best = std::numeric_limits<double>::max();
    const auto consider = [&](EngravingItem* item) {
        if (!item) {
            return;
        }
        const PointF center = item->canvasBoundingRect().center();
        const double dx = center.x() - logicalPosition.x();
        const double dy = center.y() - logicalPosition.y();
        const double distance = dx * dx + dy * dy;
        if (distance < best) {
            best = distance;
            closest = item;
        }
    };

    // Expressions may be anchored to either an actual note or a rest.
    for (EngravingItem* item : hitItems) {
        if (item && (item->isNote() || item->isRest())) {
            consider(item);
        }
    }
    for (EngravingItem* item : hitItems) {
        if (item && item->isChord()) {
            for (Note* note : toChord(item)->notes()) {
                consider(note);
            }
        }
    }
    return closest;
}

bool RepriseExpressionInputController::selectionContains(const EngravingItem* target) const
{
    if (!target || !m_view || !m_view->notationInteraction()) {
        return false;
    }
    const INotationSelectionPtr selection = m_view->notationInteraction()->selection();
    if (!selection) {
        return false;
    }
    const auto& elements = selection->elements();
    return std::find(elements.cbegin(), elements.cend(), target) != elements.cend();
}

void RepriseExpressionInputController::ensureSelected(EngravingItem* target)
{
    if (!target || !m_view || selectionContains(target)) {
        return;
    }
    const INotationInteractionPtr interaction = m_view->notationInteraction();
    if (!interaction) {
        return;
    }
    interaction->select({ target }, SelectType::REPLACE, target->staffIdx());
}

bool RepriseExpressionInputController::handleMousePress(QMouseEvent* event)
{
    if (event->button() != Qt::LeftButton || event->modifiers() != Qt::NoModifier) {
        return false;
    }
    EngravingItem* target = targetAt(event->position());
    if (!target) {
        return false;
    }

    // If the note already belongs to a list/range selection, do not let the
    // first half of the double click collapse that selection.
    ensureSelected(target);
    m_ownedPress = true;
    m_requestOnRelease = false;
    event->accept();
    return true;
}

bool RepriseExpressionInputController::handleMouseDoubleClick(QMouseEvent* event)
{
    if (event->button() != Qt::LeftButton || event->modifiers() != Qt::NoModifier) {
        return false;
    }
    EngravingItem* target = targetAt(event->position());
    if (!target) {
        return false;
    }

    ensureSelected(target);
    m_ownedPress = true;
    m_requestOnRelease = true;
    event->accept();
    return true;
}

bool RepriseExpressionInputController::handleMouseMove(QMouseEvent* event)
{
    if (!m_ownedPress) {
        return false;
    }
    if (!(event->buttons() & Qt::LeftButton)) {
        resetPointerOwnership();
        return false;
    }
    event->accept();
    return true;
}

bool RepriseExpressionInputController::handleMouseRelease(QMouseEvent* event)
{
    if (!m_ownedPress || event->button() != Qt::LeftButton) {
        return false;
    }

    const bool requestExpression = m_requestOnRelease;
    resetPointerOwnership();
    event->accept();
    if (requestExpression) {
        emit expressionRequested();
    }
    return true;
}

void RepriseExpressionInputController::resetPointerOwnership()
{
    m_ownedPress = false;
    m_requestOnRelease = false;
}

bool RepriseExpressionInputController::eventFilter(QObject* watched, QEvent* event)
{
    if (watched != m_view) {
        return QObject::eventFilter(watched, event);
    }

    if (!inputAvailable()) {
        resetPointerOwnership();
        return QObject::eventFilter(watched, event);
    }

    switch (event->type()) {
    case QEvent::MouseButtonPress:
        return handleMousePress(static_cast<QMouseEvent*>(event));
    case QEvent::MouseButtonDblClick:
        return handleMouseDoubleClick(static_cast<QMouseEvent*>(event));
    case QEvent::MouseMove:
        return handleMouseMove(static_cast<QMouseEvent*>(event));
    case QEvent::MouseButtonRelease:
        return handleMouseRelease(static_cast<QMouseEvent*>(event));
    case QEvent::FocusOut:
    case QEvent::UngrabMouse:
        resetPointerOwnership();
        break;
    default:
        break;
    }

    return QObject::eventFilter(watched, event);
}
