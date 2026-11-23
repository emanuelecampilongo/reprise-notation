/*
 * SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Reprise Notation contributors
 */
#pragma once

#include <QObject>
#include <qqmlintegration.h>

#include "notationscene/reprise/reprisetoolstate.h"

namespace mu::notation {
class RepriseToolModel : public QObject
{
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(bool enabled READ enabled CONSTANT)
    Q_PROPERTY(int activeTool READ activeTool NOTIFY stateChanged)
    Q_PROPERTY(bool operationActive READ operationActive NOTIFY stateChanged)
    Q_PROPERTY(bool canCancel READ canCancel NOTIFY stateChanged)
    Q_PROPERTY(int activeShape READ activeShape NOTIFY stateChanged)

public:
    enum Tool { Selection = 0, SmartShape = 1, Articulation = 2, TimeSignature = 3, Clef = 4, KeySignature = 5, Expression = 6, Repeat = 7, Measure = 8 };
    Q_ENUM(Tool)

    // Reprise mirrors Finale's regular Smart Shape palette entries while
    // keeping MuseScore-native engraving objects underneath. Bend Hat,
    // Dashed Curve and Custom Line are intentionally omitted: the first two
    // have no clean one-to-one native representation, while Finale's Custom
    // Line is a user-defined preset/editor rather than a fixed shape.
    enum Shape {
        Slur = 0,
        Crescendo = 1,
        DashedSlur = 2,
        Decrescendo = 3,
        Trill = 4,
        TrillExtension = 5,
        Ottava8va = 6,
        Ottava15ma = 7,
        BracketBoth = 8,
        BracketBothDashed = 9,
        BracketEnd = 10,
        BracketEndDashed = 11,
        Line = 12,
        DashedLine = 13,
        Glissando = 14,
        GuitarBend = 15,
        TabSlide = 16,
    };
    Q_ENUM(Shape)

    explicit RepriseToolModel(QObject* parent = nullptr);

    bool enabled() const { return m_enabled; }
    int activeTool() const;
    bool operationActive() const { return m_state.operationActive(); }
    bool canCancel() const;

    int activeShape() const { return m_activeShape; }
    Q_INVOKABLE bool selectShape(int shape);
    Q_INVOKABLE bool selectTool(int tool);
    Q_INVOKABLE bool beginOperation();
    Q_INVOKABLE void finishOperation();
    Q_INVOKABLE bool cancel(bool nativeOwnsEscape);
    Q_INVOKABLE void reset();

signals:
    void stateChanged();

private:
    reprise::ToolState m_state;
    const bool m_enabled;
    Shape m_activeShape = Slur;
};
}
