/*
 * SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Reprise Notation contributors
 */
#include "reprisetoolmodel.h"

#include <QtGlobal>

using namespace mu::notation;

RepriseToolModel::RepriseToolModel(QObject* parent)
    : QObject(parent), m_enabled(qgetenv("REPRISE_TOOLS") != "0")
{
}

int RepriseToolModel::activeTool() const
{
    switch (m_state.activeTool()) {
    case reprise::Tool::SmartShape:
        return SmartShape;
    case reprise::Tool::Articulation:
        return Articulation;
    case reprise::Tool::TimeSignature:
        return TimeSignature;
    case reprise::Tool::Clef:
        return Clef;
    case reprise::Tool::KeySignature:
        return KeySignature;
    case reprise::Tool::Expression:
        return Expression;
    case reprise::Tool::Repeat:
        return Repeat;
    case reprise::Tool::Measure:
        return Measure;
    default:
        return Selection;
    }
}

bool RepriseToolModel::canCancel() const
{
    return m_enabled && (m_state.operationActive() || m_state.activeTool() != reprise::Tool::Selection);
}

bool RepriseToolModel::selectShape(int shape)
{
    if (!m_enabled || shape < Slur || shape > TabSlide) {
        return false;
    }
    if (shape == m_activeShape) {
        return true;
    }
    m_state.finishOperation();
    m_activeShape = static_cast<Shape>(shape);
    emit stateChanged();
    return true;
}

bool RepriseToolModel::selectTool(int tool)
{
    if (!m_enabled || (tool != Selection && tool != SmartShape && tool != Articulation
                       && tool != TimeSignature && tool != Clef && tool != KeySignature
                       && tool != Expression && tool != Repeat && tool != Measure)) {
        return false;
    }

    reprise::Tool target = reprise::Tool::Selection;
    if (tool == SmartShape) {
        target = reprise::Tool::SmartShape;
    } else if (tool == Articulation) {
        target = reprise::Tool::Articulation;
    } else if (tool == TimeSignature) {
        target = reprise::Tool::TimeSignature;
    } else if (tool == Clef) {
        target = reprise::Tool::Clef;
    } else if (tool == KeySignature) {
        target = reprise::Tool::KeySignature;
    } else if (tool == Expression) {
        target = reprise::Tool::Expression;
    } else if (tool == Repeat) {
        target = reprise::Tool::Repeat;
    } else if (tool == Measure) {
        target = reprise::Tool::Measure;
    }

    const auto previousTool = m_state.activeTool();
    const bool previousOperation = m_state.operationActive();
    m_state.activateTool(target);
    if (previousTool != m_state.activeTool() || previousOperation) {
        emit stateChanged();
    }
    return true;
}

bool RepriseToolModel::beginOperation()
{
    if (!m_enabled || !m_state.beginOperation()) {
        return false;
    }
    emit stateChanged();
    return true;
}

void RepriseToolModel::finishOperation()
{
    if (m_state.operationActive()) {
        m_state.finishOperation();
        emit stateChanged();
    }
}

bool RepriseToolModel::cancel(bool nativeOwnsEscape)
{
    if (!m_enabled || m_state.escape(nativeOwnsEscape) == reprise::EscapeResult::DelegateToNative) {
        return false;
    }
    emit stateChanged();
    return true;
}

void RepriseToolModel::reset()
{
    if (m_state.activeTool() != reprise::Tool::Selection || m_state.operationActive()) {
        m_state.reset();
        emit stateChanged();
    }
}
