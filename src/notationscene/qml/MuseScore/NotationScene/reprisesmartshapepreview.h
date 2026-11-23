/*
 * SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Reprise Notation contributors
 */
#pragma once

#include <algorithm>
#include <cmath>

#include <QFontMetricsF>
#include <QLineF>
#include <QPainter>
#include <QPainterPath>

#include "reprisesmartshapeadapter.h"

namespace mu::notation::reprise::smartshape::preview {

inline QFont previewFont(const QPainter* painter, int pixelSize, bool italic = false)
{
    QFont font = painter ? painter->font() : QFont();
    font.setPixelSize(pixelSize);
    font.setItalic(italic);
    return font;
}

inline void drawHorizontalWave(QPainter* painter, qreal x1, qreal x2, qreal y)
{
    if (!painter || qFuzzyCompare(x1, x2)) {
        return;
    }
    if (x2 < x1) {
        std::swap(x1, x2);
    }

    constexpr qreal HALF_WAVE = 4.0;
    constexpr qreal AMPLITUDE = 2.6;
    QPainterPath path(QPointF(x1, y));
    qreal x = x1;
    bool above = true;
    while (x < x2) {
        const qreal next = std::min(x + HALF_WAVE, x2);
        const qreal mid = (x + next) * 0.5;
        path.quadTo(QPointF(mid, y + (above ? -AMPLITUDE : AMPLITUDE)), QPointF(next, y));
        above = !above;
        x = next;
    }
    painter->drawPath(path);
}

inline void drawArrowHead(QPainter* painter, const QPointF& tip, const QPointF& from)
{
    QPointF direction = tip - from;
    const qreal length = QLineF(QPointF(), direction).length();
    if (!painter || length < 0.5) {
        return;
    }
    direction /= length;
    const QPointF normal(-direction.y(), direction.x());
    constexpr qreal ARROW_LENGTH = 8.0;
    constexpr qreal ARROW_HALF_WIDTH = 4.0;
    painter->drawLine(tip, tip - direction * ARROW_LENGTH + normal * ARROW_HALF_WIDTH);
    painter->drawLine(tip, tip - direction * ARROW_LENGTH - normal * ARROW_HALF_WIDTH);
}

inline qreal targetRingRadius(const QRectF& noteRect)
{
    const qreal noteSize = std::max(noteRect.width(), noteRect.height());
    return std::clamp(noteSize * 0.9 + 2.0, 5.0, 24.0);
}

inline void drawTargetRing(QPainter* painter, const QColor& color, const QRectF& noteRect)
{
    if (!painter || noteRect.isEmpty()) {
        return;
    }

    const QPointF center = noteRect.center();
    const qreal radius = targetRingRadius(noteRect);
    QPen ringPen(color, std::clamp(radius * 0.16, 1.2, 2.6), Qt::SolidLine);
    painter->setPen(ringPen);
    QColor fill = color;
    fill.setAlpha(28);
    painter->setBrush(fill);
    painter->drawEllipse(QRectF(center.x() - radius, center.y() - radius, radius * 2.0, radius * 2.0));
    painter->setBrush(Qt::NoBrush);
}

inline void drawInvalidTarget(QPainter* painter, const QColor& color, const QPointF& point)
{
    if (!painter) {
        return;
    }
    // Deliberately use only a simple X. Combining an X with an enclosing circle
    // produced an unfortunate visual shape at small sizes.
    QPen pen(color, 1.9, Qt::SolidLine);
    painter->setPen(pen);
    painter->setBrush(Qt::NoBrush);
    painter->drawLine(point + QPointF(-5.0, -5.0), point + QPointF(5.0, 5.0));
    painter->drawLine(point + QPointF(-5.0, 5.0), point + QPointF(5.0, -5.0));
}

inline void paint(QPainter* painter, int shape, const QColor& color, const QPointF& start, const QPointF& end,
                  qreal slurArcDirection, bool validTarget, const QRectF& startRect, const QRectF& targetRect)
{
    if (!painter) {
        return;
    }

    const bool noteAnchored = isNoteAnchoredShape(shape);
    const bool slurShape = isSlurShape(shape);
    const bool hairpinShape = isHairpinShape(shape);
    const bool dashedPreset = isDashedShape(shape);
    // For note-to-note shapes the target rectangle is the visual source of truth.
    // Slurs are intentionally never presented as invalid: Finale-style slurs can
    // still resolve to a nearby/final note or to a free end in the supported case.
    const bool effectiveValidTarget = noteAnchored
                                      ? (slurShape || !targetRect.isEmpty())
                                      : validTarget;

    painter->save();
    painter->setWorldTransform(QTransform());
    painter->setRenderHint(QPainter::Antialiasing);
    painter->setBrush(Qt::NoBrush);
    painter->setFont(previewFont(painter, 13));

    QPen pen(color, 1.8, (dashedPreset || !effectiveValidTarget) ? Qt::DashLine : Qt::SolidLine);
    painter->setPen(pen);

    const qreal directDistance = QLineF(start, end).length();
    const qreal horizontalDistance = std::abs(end.x() - start.x());

    if (hairpinShape && horizontalDistance > 2.0) {
        const qreal y = start.y() + 24.0;
        const QPointF left(std::min(start.x(), end.x()), y);
        const QPointF right(std::max(start.x(), end.x()), y);
        if (shape == RepriseToolModel::Decrescendo) {
            painter->drawLine(left + QPointF(0.0, -6.0), right);
            painter->drawLine(left + QPointF(0.0, 6.0), right);
        } else {
            painter->drawLine(left, right + QPointF(0.0, -6.0));
            painter->drawLine(left, right + QPointF(0.0, 6.0));
        }
    } else if (slurShape && directDistance > 2.0) {
        const QPointF delta = end - start;
        const qreal arc = slurArcDirection * std::clamp(directDistance * 0.18, 12.0, 48.0);
        QPainterPath path(start);
        path.cubicTo(start + delta / 3.0 + QPointF(0.0, arc),
                     start + delta * (2.0 / 3.0) + QPointF(0.0, arc), end);
        painter->drawPath(path);
    } else if ((shape == RepriseToolModel::Trill || shape == RepriseToolModel::TrillExtension)
               && horizontalDistance > 2.0) {
        const qreal y = start.y() - 18.0;
        qreal waveStartX = start.x();
        if (shape == RepriseToolModel::Trill) {
            const QFont font = previewFont(painter, 14, true);
            painter->setFont(font);
            const QString label = QStringLiteral("tr");
            painter->drawText(QPointF(start.x(), y + 4.0), label);
            waveStartX += QFontMetricsF(font).horizontalAdvance(label) + 4.0;
        }
        drawHorizontalWave(painter, waveStartX, end.x(), y);
    } else if ((shape == RepriseToolModel::Ottava8va || shape == RepriseToolModel::Ottava15ma)
               && horizontalDistance > 2.0) {
        const qreal y = start.y() - 18.0;
        const QString label = shape == RepriseToolModel::Ottava15ma ? QStringLiteral("15ma") : QStringLiteral("8va");
        const QFont font = previewFont(painter, 14, true);
        painter->setFont(font);
        painter->drawText(QPointF(start.x(), y + 4.0), label);
        const qreal lineStartX = start.x() + QFontMetricsF(font).horizontalAdvance(label) + 5.0;
        painter->drawLine(QPointF(lineStartX, y), QPointF(end.x(), y));
        painter->drawLine(QPointF(end.x(), y), QPointF(end.x(), y + 8.0));
    } else if ((shape == RepriseToolModel::BracketBoth || shape == RepriseToolModel::BracketBothDashed
                || shape == RepriseToolModel::BracketEnd || shape == RepriseToolModel::BracketEndDashed)
               && horizontalDistance > 2.0) {
        const qreal y = start.y() + 18.0;
        const qreal x1 = start.x();
        const qreal x2 = end.x();
        painter->drawLine(QPointF(x1, y), QPointF(x2, y));
        if (shape == RepriseToolModel::BracketBoth || shape == RepriseToolModel::BracketBothDashed) {
            painter->drawLine(QPointF(x1, y), QPointF(x1, y + 8.0));
        }
        painter->drawLine(QPointF(x2, y), QPointF(x2, y + 8.0));
    } else if ((shape == RepriseToolModel::Line || shape == RepriseToolModel::DashedLine)
               && horizontalDistance > 2.0) {
        painter->drawLine(QPointF(start.x(), start.y() + 18.0), QPointF(end.x(), start.y() + 18.0));
    } else if (shape == RepriseToolModel::Glissando && directDistance > 2.0) {
        painter->drawLine(start, end);
        const QPointF middle = (start + end) * 0.5;
        painter->setFont(previewFont(painter, 12, true));
        painter->drawText(middle + QPointF(4.0, -4.0), QStringLiteral("gliss."));
    } else if (shape == RepriseToolModel::TabSlide && directDistance > 2.0) {
        painter->drawLine(start, end);
    } else if (shape == RepriseToolModel::GuitarBend && directDistance > 2.0) {
        const QPointF delta = end - start;
        const QPointF control1 = start + delta * 0.35 + QPointF(0.0, -18.0);
        const QPointF control2 = start + delta * 0.75 + QPointF(0.0, -18.0);
        QPainterPath path(start);
        path.cubicTo(control1, control2, end);
        painter->drawPath(path);
        drawArrowHead(painter, end, control2);
    } else if (directDistance > 2.0) {
        painter->drawLine(start, end);
    }

    if (noteAnchored) {
        drawTargetRing(painter, color, startRect);
        if (!targetRect.isEmpty()) {
            drawTargetRing(painter, color, targetRect);
        } else if (!slurShape && !validTarget && directDistance > 4.0) {
            drawInvalidTarget(painter, color, end);
        }
    }

    painter->restore();
}

} // namespace mu::notation::reprise::smartshape::preview
