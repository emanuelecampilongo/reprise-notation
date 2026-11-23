/*
 * SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Reprise Notation contributors
 */
#include "reprisesmartshapepreviewmodel.h"

#include <memory>
#include <utility>

#include "reprisetoolmodel.h"

#include "engraving/dom/factory.h"
#include "engraving/dom/glissando.h"
#include "engraving/dom/hairpin.h"
#include "engraving/dom/masterscore.h"
#include "engraving/dom/ottava.h"
#include "engraving/dom/slur.h"
#include "engraving/dom/textline.h"
#include "engraving/dom/trill.h"

using namespace mu::notation;
using namespace mu::engraving;

RepriseSmartShapePreviewModel::RepriseSmartShapePreviewModel(QObject* parent)
    : QObject(parent)
{
}

QVariant RepriseSmartShapePreviewModel::previewElement(int shape)
{
    // The palette score is initialized by MuseScore's engraving subsystem.
    // Do not cache a failed lookup while QML and the modules are starting.
    if (!gpaletteScore) {
        return QVariant();
    }

    auto it = m_elements.constFind(shape);
    if (it == m_elements.cend()) {
        engraving::ElementPtr element = makePreviewElement(shape);
        if (!element) {
            return QVariant();
        }
        it = m_elements.insert(shape, std::move(element));
    }
    return QVariant::fromValue(it.value());
}

engraving::ElementPtr RepriseSmartShapePreviewModel::makePreviewElement(int shape) const
{
    EngravingItem* parent = gpaletteScore->dummy();
    switch (shape) {
    case RepriseToolModel::Slur:
    case RepriseToolModel::DashedSlur: {
        auto slur = Factory::makeSlur(parent);
        if (shape == RepriseToolModel::DashedSlur) {
            slur->setStyleType(SlurStyleType::Dashed);
        }
        return slur;
    }

    case RepriseToolModel::Crescendo:
    case RepriseToolModel::Decrescendo: {
        auto hairpin = Factory::makeHairpin(parent);
        hairpin->setHairpinType(shape == RepriseToolModel::Crescendo
                                ? HairpinType::CRESC_HAIRPIN : HairpinType::DIM_HAIRPIN);
        return hairpin;
    }

    case RepriseToolModel::Trill:
    case RepriseToolModel::TrillExtension: {
        std::shared_ptr<Trill> trill(Factory::createTrill(parent));
        // The extension has no initial ornament, matching the real tool.
        if (shape == RepriseToolModel::Trill) {
            trill->setTrillType(TrillType::TRILL_LINE);
        }
        return trill;
    }

    case RepriseToolModel::Ottava8va:
    case RepriseToolModel::Ottava15ma: {
        std::shared_ptr<Ottava> ottava(Factory::createOttava(parent));
        ottava->setOttavaType(shape == RepriseToolModel::Ottava8va
                              ? OttavaType::OTTAVA_8VA : OttavaType::OTTAVA_15MA);
        ottava->styleChanged();
        return ottava;
    }

    case RepriseToolModel::BracketBoth:
    case RepriseToolModel::BracketBothDashed:
    case RepriseToolModel::BracketEnd:
    case RepriseToolModel::BracketEndDashed:
    case RepriseToolModel::Line:
    case RepriseToolModel::DashedLine: {
        auto line = Factory::makeTextLine(parent);
        line->setLineVisible(true);
        const bool dashed = shape == RepriseToolModel::BracketBothDashed
                            || shape == RepriseToolModel::BracketEndDashed
                            || shape == RepriseToolModel::DashedLine;
        line->setLineStyle(dashed ? LineType::DASHED : LineType::SOLID);
        if (dashed) {
            line->setPropertyFlags(Pid::LINE_STYLE, PropertyFlags::UNSTYLED);
        }
        const bool beginHook = shape == RepriseToolModel::BracketBoth
                               || shape == RepriseToolModel::BracketBothDashed;
        const bool endHook = beginHook
                             || shape == RepriseToolModel::BracketEnd
                             || shape == RepriseToolModel::BracketEndDashed;
        line->setBeginHookType(beginHook ? HookType::HOOK_90 : HookType::NONE);
        line->setEndHookType(endHook ? HookType::HOOK_90 : HookType::NONE);
        line->setLen(6 * gpaletteScore->style().spatium());
        return line;
    }

    case RepriseToolModel::Glissando:
    case RepriseToolModel::TabSlide: {
        auto glissando = Factory::makeGlissando(parent);
        if (shape == RepriseToolModel::TabSlide) {
            glissando->setGlissandoType(GlissandoType::STRAIGHT);
            glissando->setPropertyFlags(Pid::GLISS_TYPE, PropertyFlags::UNSTYLED);
            glissando->setText(u"");
            glissando->setShowText(false);
            glissando->setGlissandoShift(true);
        }
        return glissando;
    }

    // Guitar bends are tied to a pair of notes. A detached palette object
    // is not equivalent; retain the current preview until it is available.
    case RepriseToolModel::GuitarBend:
    default:
        return {};
    }
}
