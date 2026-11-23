/*
 * SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Reprise Notation contributors
 */
#include "repriseclefinputcontroller.h"

#include <algorithm>

#include <QEvent>
#include <QMouseEvent>

#include "abstractnotationpaintview.h"
#include "reprisetoolmodel.h"

#include "engraving/dom/engravingitem.h"
#include "engraving/dom/measure.h"
#include "engraving/dom/staff.h"
#include "notation/inotationinteraction.h"

using namespace mu;
using namespace mu::notation;
using namespace mu::engraving;

namespace {
constexpr qreal CLEF_HIT_RADIUS_PX = 3.0;
}

RepriseClefInputController::RepriseClefInputController(QObject* parent)
    : QObject(parent)
{
}

RepriseClefInputController::~RepriseClefInputController()
{
    if (m_view) {
        m_view->removeEventFilter(this);
    }
}

QObject* RepriseClefInputController::view() const
{
    return m_view.data();
}

void RepriseClefInputController::setView(QObject* view)
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

RepriseToolModel* RepriseClefInputController::toolModel() const
{
    return m_toolModel.data();
}

void RepriseClefInputController::setToolModel(RepriseToolModel* model)
{
    if (m_toolModel == model) {
        return;
    }

    QObject::disconnect(m_toolStateConnection);
    resetPointerOwnership();
    m_toolModel = model;
    if (m_toolModel) {
        m_toolStateConnection = connect(m_toolModel, &RepriseToolModel::stateChanged, this, [this]() {
            if (!m_toolModel || m_toolModel->activeTool() != RepriseToolModel::Clef) {
                resetPointerOwnership();
            }
        });
    }
    emit toolModelChanged();
}

bool RepriseClefInputController::inputAvailable() const
{
    if (!m_view || !m_toolModel || !m_toolModel->enabled()
        || m_toolModel->activeTool() != RepriseToolModel::Clef
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

Measure* RepriseClefInputController::measureAt(const QPointF& viewPosition, staff_idx_t* staffIdx) const
{
    if (!m_view) {
        return nullptr;
    }

    const INotationInteractionPtr interaction = m_view->notationInteraction();
    if (!interaction) {
        return nullptr;
    }

    const PointF logicalPosition = m_view->toLogical(viewPosition);
    Staff* staff = interaction->hitStaff(logicalPosition);
    if (!staff) {
        return nullptr;
    }

    if (staffIdx) {
        *staffIdx = staff->idx();
    }

    const qreal scale = std::max(m_view->currentScaling(), 0.001);
    const float hitWidth = static_cast<float>(CLEF_HIT_RADIUS_PX / scale);
    const std::vector<EngravingItem*> hitItems = interaction->hitElements(logicalPosition, hitWidth);

    for (auto it = hitItems.crbegin(); it != hitItems.crend(); ++it) {
        EngravingItem* item = *it;
        if (!item) {
            continue;
        }
        if (item->isMeasure()) {
            return toMeasure(item);
        }
        if (Measure* measure = item->findMeasure()) {
            return measure;
        }
    }

    return nullptr;
}

void RepriseClefInputController::selectMeasure(Measure* measure, staff_idx_t staffIdx)
{
    if (!measure || !m_view) {
        return;
    }
    const INotationInteractionPtr interaction = m_view->notationInteraction();
    if (!interaction) {
        return;
    }

    // Keep the target as a native MuseScore measure/staff selection so clef
    // placement, replacement, undo/redo and courtesy behavior remain native.
    interaction->select({ measure }, SelectType::SINGLE, staffIdx);
}

bool RepriseClefInputController::handleMousePress(QMouseEvent* event)
{
    if (event->button() != Qt::LeftButton || event->modifiers() != Qt::NoModifier) {
        return false;
    }

    staff_idx_t staffIdx = muse::nidx;
    Measure* measure = measureAt(event->position(), &staffIdx);
    if (!measure || staffIdx == muse::nidx) {
        return false;
    }

    selectMeasure(measure, staffIdx);
    m_ownedPress = true;
    m_requestOnRelease = false;
    event->accept();
    return true;
}

bool RepriseClefInputController::handleMouseDoubleClick(QMouseEvent* event)
{
    if (event->button() != Qt::LeftButton || event->modifiers() != Qt::NoModifier) {
        return false;
    }

    staff_idx_t staffIdx = muse::nidx;
    Measure* measure = measureAt(event->position(), &staffIdx);
    if (!measure || staffIdx == muse::nidx) {
        return false;
    }

    selectMeasure(measure, staffIdx);
    m_ownedPress = true;
    m_requestOnRelease = true;
    event->accept();
    return true;
}

bool RepriseClefInputController::handleMouseMove(QMouseEvent* event)
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

bool RepriseClefInputController::handleMouseRelease(QMouseEvent* event)
{
    if (!m_ownedPress || event->button() != Qt::LeftButton) {
        return false;
    }

    const bool requestClef = m_requestOnRelease;
    resetPointerOwnership();
    event->accept();
    if (requestClef) {
        emit clefRequested();
    }
    return true;
}

void RepriseClefInputController::resetPointerOwnership()
{
    m_ownedPress = false;
    m_requestOnRelease = false;
}

bool RepriseClefInputController::eventFilter(QObject* watched, QEvent* event)
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
