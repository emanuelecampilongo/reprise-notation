/*
 * SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Reprise Notation contributors
 */
#include "reprisearticulationinputcontroller.h"

#include <algorithm>
#include <limits>

#include <QEvent>
#include <QGuiApplication>
#include <QMouseEvent>
#include <QStyleHints>

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
constexpr qreal ARTICULATION_NOTE_HIT_RADIUS_PX = 16.0;
}

RepriseArticulationInputController::RepriseArticulationInputController(QObject* parent)
    : QObject(parent)
{
}

RepriseArticulationInputController::~RepriseArticulationInputController()
{
    resetPointerOwnership();
    if (m_view) {
        m_view->removeEventFilter(this);
    }
}

QObject* RepriseArticulationInputController::view() const
{
    return m_view.data();
}

void RepriseArticulationInputController::setView(QObject* view)
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

RepriseToolModel* RepriseArticulationInputController::toolModel() const
{
    return m_toolModel.data();
}

void RepriseArticulationInputController::setToolModel(RepriseToolModel* model)
{
    if (m_toolModel == model) {
        return;
    }

    QObject::disconnect(m_toolStateConnection);
    resetPointerOwnership();
    m_toolModel = model;
    if (m_toolModel) {
        m_toolStateConnection = connect(m_toolModel, &RepriseToolModel::stateChanged, this, [this]() {
            if (!m_toolModel || m_toolModel->activeTool() != RepriseToolModel::Articulation) {
                resetPointerOwnership();
            }
        });
    }
    emit toolModelChanged();
}

bool RepriseArticulationInputController::inputAvailable() const
{
    if (!m_view || !m_toolModel || !m_toolModel->enabled()
        || m_toolModel->activeTool() != RepriseToolModel::Articulation
        || !m_view->isMainView() || m_view->publishMode() || !m_view->hasActiveFocus()
        || m_view->isNoteEnterMode()) {
        return false;
    }

    const INotationInteractionPtr interaction = m_view->notationInteraction();
    return interaction
           && !interaction->isTextEditingStarted()
           && !interaction->isGripEditStarted()
           && !interaction->isEditingElement()
           && (!interaction->isDragStarted() || m_lassoInteraction.lock() == interaction)
           && !interaction->isOutgoingDragStarted();
}

Note* RepriseArticulationInputController::noteAt(const QPointF& viewPosition) const
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
    const float hitWidth = static_cast<float>(ARTICULATION_NOTE_HIT_RADIUS_PX / scale);
    const std::vector<EngravingItem*> hitItems = interaction->hitElements(logicalPosition, hitWidth);

    // Prefer an actual notehead whenever MuseScore reports one in the hit area.
    Note* nearestNote = nullptr;
    double nearestDistance = std::numeric_limits<double>::max();
    auto consider = [&](Note* note) {
        if (!note) {
            return;
        }
        const PointF center = note->canvasBoundingRect().center();
        const double dx = center.x() - logicalPosition.x();
        const double dy = center.y() - logicalPosition.y();
        const double distance = dx * dx + dy * dy;
        if (distance < nearestDistance) {
            nearestDistance = distance;
            nearestNote = note;
        }
    };

    for (EngravingItem* item : hitItems) {
        if (item && item->isNote()) {
            consider(toNote(item));
        }
    }
    if (nearestNote) {
        return nearestNote;
    }

    // A chord can be topmost in MuseScore's hit list even when the pointer is
    // visually on one of its noteheads. Resolve that chord to its nearest note.
    for (EngravingItem* item : hitItems) {
        if (!item || !item->isChord()) {
            continue;
        }
        for (Note* note : toChord(item)->notes()) {
            consider(note);
        }
    }
    return nearestNote;
}

bool RepriseArticulationInputController::selectionContains(const Note* note) const
{
    if (!note || !m_view || !m_view->notationInteraction()) {
        return false;
    }
    const INotationSelectionPtr selection = m_view->notationInteraction()->selection();
    if (!selection) {
        return false;
    }
    const std::vector<Note*> notes = selection->notes();
    return notes.size() == 1 && notes.front() == note;
}

void RepriseArticulationInputController::ensureSelected(Note* note)
{
    if (!note || !m_view || selectionContains(note)) {
        return;
    }
    const INotationInteractionPtr interaction = m_view->notationInteraction();
    if (!interaction) {
        return;
    }
    interaction->select({ note }, SelectType::REPLACE, note->staffIdx());
}

bool RepriseArticulationInputController::handleMousePress(QMouseEvent* event)
{
    if (event->button() != Qt::LeftButton
        || (event->modifiers() != Qt::NoModifier && event->modifiers() != Qt::ShiftModifier)) {
        return false;
    }
    const PointF logicalPosition = m_view->toLogical(event->position());
    const EngravingItem* hit = m_view->notationInteraction()->hitElement(logicalPosition, 0.0f);
    if (hit && hit->isArticulationOrFermata()) {
        return false;
    }
    m_ownedPress = true;
    m_doubleClick = event->type() == QEvent::MouseButtonDblClick;
    m_pressPosition = event->position();
    event->accept();
    return true;
}

bool RepriseArticulationInputController::handleMouseMove(QMouseEvent* event)
{
    if (!m_ownedPress) {
        return false;
    }
    if (!(event->buttons() & Qt::LeftButton)) {
        resetPointerOwnership();
        return false;
    }
    auto interaction = m_lassoInteraction.lock();
    if (!interaction && (event->position() - m_pressPosition).manhattanLength() >= QGuiApplication::styleHints()->startDragDistance()) {
        interaction = m_view->notationInteraction();
        interaction->startDrag({}, PointF(), [](const EngravingItem*) { return false; });
        m_lassoInteraction = interaction;
    }
    if (interaction) {
        interaction->drag(m_view->toLogical(m_pressPosition), m_view->toLogical(event->position()), DragMode::BothXY);
    }
    event->accept();
    return true;
}

bool RepriseArticulationInputController::handleMouseRelease(QMouseEvent* event)
{
    if (!m_ownedPress || event->button() != Qt::LeftButton) {
        return false;
    }

    const auto interaction = m_lassoInteraction.lock();
    const QPointF pressPosition = m_pressPosition;
    const bool doubleClick = m_doubleClick;
    std::vector<Note*> enclosedNotes;
    if (interaction) {
        interaction->drag(m_view->toLogical(pressPosition), m_view->toLogical(event->position()), DragMode::BothXY);
        enclosedNotes = interaction->selection()->notes();
    }
    resetPointerOwnership();
    event->accept();

    if (interaction) {
        // Native lasso release expands the selection to a range. Keep the
        // notes actually enclosed so the palette applies only to that group.
        if (!enclosedNotes.empty()) {
            const std::vector<EngravingItem*> targets(enclosedNotes.begin(), enclosedNotes.end());
            interaction->select(targets, SelectType::REPLACE, enclosedNotes.front()->staffIdx());
            emit articulationRequested();
        }
    } else if ((event->position() - pressPosition).manhattanLength() < QGuiApplication::styleHints()->startDragDistance()) {
        if (Note* note = noteAt(pressPosition)) {
            ensureSelected(note);
            if (doubleClick) {
                emit articulationRequested();
            }
        }
    }
    return true;
}

void RepriseArticulationInputController::resetPointerOwnership()
{
    m_ownedPress = false;
    m_doubleClick = false;
    const auto interaction = m_lassoInteraction.lock();
    m_lassoInteraction.reset();
    if (interaction && m_view && interaction == m_view->notationInteraction()) {
        interaction->endDrag();
    }
}

bool RepriseArticulationInputController::eventFilter(QObject* watched, QEvent* event)
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
        return handleMousePress(static_cast<QMouseEvent*>(event));
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
