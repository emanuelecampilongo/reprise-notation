/*
 * SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Reprise Notation contributors
 */
#include "reprisetimesignatureinputcontroller.h"

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
constexpr qreal TIME_SIGNATURE_HIT_RADIUS_PX = 3.0;
}

RepriseTimeSignatureInputController::RepriseTimeSignatureInputController(QObject* parent)
    : QObject(parent)
{
}

RepriseTimeSignatureInputController::~RepriseTimeSignatureInputController()
{
    if (m_view) {
        m_view->removeEventFilter(this);
    }
}

QObject* RepriseTimeSignatureInputController::view() const
{
    return m_view.data();
}

void RepriseTimeSignatureInputController::setView(QObject* view)
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

RepriseToolModel* RepriseTimeSignatureInputController::toolModel() const
{
    return m_toolModel.data();
}

void RepriseTimeSignatureInputController::setToolModel(RepriseToolModel* model)
{
    if (m_toolModel == model) {
        return;
    }

    QObject::disconnect(m_toolStateConnection);
    resetPointerOwnership();
    m_toolModel = model;
    if (m_toolModel) {
        m_toolStateConnection = connect(m_toolModel, &RepriseToolModel::stateChanged, this, [this]() {
            if (!m_toolModel || m_toolModel->activeTool() != RepriseToolModel::TimeSignature) {
                resetPointerOwnership();
            }
        });
    }
    emit toolModelChanged();
}

bool RepriseTimeSignatureInputController::inputAvailable() const
{
    if (!m_view || !m_toolModel || !m_toolModel->enabled()
        || m_toolModel->activeTool() != RepriseToolModel::TimeSignature
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

Measure* RepriseTimeSignatureInputController::measureAt(const QPointF& viewPosition, staff_idx_t* staffIdx) const
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
    const float hitWidth = static_cast<float>(TIME_SIGNATURE_HIT_RADIUS_PX / scale);
    const std::vector<EngravingItem*> hitItems = interaction->hitElements(logicalPosition, hitWidth);

    // hitElements() falls back to the measure itself when the click is on an
    // otherwise empty part of the staff. Prefer the topmost native hit and
    // resolve any note/rest/signature back to its owning measure.
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

void RepriseTimeSignatureInputController::selectMeasure(Measure* measure, staff_idx_t staffIdx)
{
    if (!measure || !m_view) {
        return;
    }
    const INotationInteractionPtr interaction = m_view->notationInteraction();
    if (!interaction) {
        return;
    }

    // This is the same native selection primitive used by MuseScore when a
    // measure is clicked. Palette application therefore retains MuseScore's
    // own time-signature semantics, undo and score-wide/staff handling.
    interaction->select({ measure }, SelectType::SINGLE, staffIdx);
}

bool RepriseTimeSignatureInputController::handleMousePress(QMouseEvent* event)
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

bool RepriseTimeSignatureInputController::handleMouseDoubleClick(QMouseEvent* event)
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

bool RepriseTimeSignatureInputController::handleMouseMove(QMouseEvent* event)
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

bool RepriseTimeSignatureInputController::handleMouseRelease(QMouseEvent* event)
{
    if (!m_ownedPress || event->button() != Qt::LeftButton) {
        return false;
    }

    const bool requestTimeSignature = m_requestOnRelease;
    resetPointerOwnership();
    event->accept();
    if (requestTimeSignature) {
        emit timeSignatureRequested();
    }
    return true;
}

void RepriseTimeSignatureInputController::resetPointerOwnership()
{
    m_ownedPress = false;
    m_requestOnRelease = false;
}

bool RepriseTimeSignatureInputController::eventFilter(QObject* watched, QEvent* event)
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
