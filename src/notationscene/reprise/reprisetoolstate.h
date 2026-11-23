/*
 * SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Reprise Notation contributors
 */
#pragma once

namespace mu::notation::reprise {

// Transient UI state with no score effects. The view adapter decides who owns Esc.
enum class Tool { Selection, NoteEntry, SmartShape, Expression, Staff, Repitch, Articulation, TimeSignature, Clef, KeySignature, Repeat, Measure };
enum class EscapeResult { DelegateToNative, OperationCancelled, SelectionActivated };

class ToolState
{
public:
    ToolState() noexcept = default;

    Tool activeTool() const noexcept { return m_tool; }
    bool operationActive() const noexcept { return m_operation; }

    void activateTool(Tool tool) noexcept
    {
        m_tool = tool;
        m_operation = false;
    }

    bool beginOperation() noexcept
    {
        if (m_tool == Tool::Selection || m_operation) {
            return false;
        }
        m_operation = true;
        return true;
    }

    void finishOperation() noexcept
    {
        m_operation = false;
    }

    void reset() noexcept
    {
        m_tool = Tool::Selection;
        m_operation = false;
    }

    EscapeResult escape(bool nativeOwnsEscape) noexcept
    {
        if (nativeOwnsEscape) {
            return EscapeResult::DelegateToNative;
        }
        if (m_operation) {
            m_operation = false;
            return EscapeResult::OperationCancelled;
        }
        if (m_tool != Tool::Selection) {
            m_tool = Tool::Selection;
            return EscapeResult::SelectionActivated;
        }
        return EscapeResult::DelegateToNative;
    }

private:
    Tool m_tool = Tool::Selection;
    bool m_operation = false;
};

} // namespace mu::notation::reprise
