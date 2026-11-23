/*
 * SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Reprise Notation contributors
 */
#pragma once

#include <utility>

#include "reprisetoolmodel.h"

#include "notation/inotation.h"

#include "engraving/dom/factory.h"
#include "engraving/dom/glissando.h"
#include "engraving/dom/guitarbend.h"
#include "engraving/dom/hairpin.h"
#include "engraving/dom/ottava.h"
#include "engraving/dom/score.h"
#include "engraving/dom/slur.h"
#include "engraving/dom/spanner.h"
#include "engraving/dom/textline.h"
#include "engraving/dom/trill.h"

namespace mu::notation::reprise::smartshape {

inline bool isSlurShape(int shape)
{
    return shape == RepriseToolModel::Slur || shape == RepriseToolModel::DashedSlur;
}

inline bool isNoteAnchoredShape(int shape)
{
    return isSlurShape(shape)
           || shape == RepriseToolModel::Glissando
           || shape == RepriseToolModel::GuitarBend
           || shape == RepriseToolModel::TabSlide;
}

inline bool isHairpinShape(int shape)
{
    return shape == RepriseToolModel::Crescendo || shape == RepriseToolModel::Decrescendo;
}

inline bool isDashedShape(int shape)
{
    return shape == RepriseToolModel::DashedSlur
           || shape == RepriseToolModel::BracketBothDashed
           || shape == RepriseToolModel::BracketEndDashed
           || shape == RepriseToolModel::DashedLine;
}

inline bool addHairpin(const INotationPtr& notation, int shape, const engraving::Fraction& startTick,
                       const engraving::Fraction& endTick, engraving::track_idx_t track)
{
    if (!notation || !notation->elements() || endTick <= startTick || !isHairpinShape(shape)) {
        return false;
    }
    auto undoStack = notation->undoStack();
    engraving::Score* score = notation->elements()->msScore();
    if (!undoStack || !score || undoStack->isLocked()) {
        return false;
    }

    const engraving::HairpinType type = shape == RepriseToolModel::Decrescendo
                                        ? engraving::HairpinType::DIM_HAIRPIN
                                        : engraving::HairpinType::CRESC_HAIRPIN;
    undoStack->prepareChanges(muse::TranslatableString("undoableAction", "Add hairpin"));
    engraving::Hairpin* hairpin = score->addHairpin(type, startTick, endTick, track);
    if (!hairpin) {
        undoStack->rollbackChanges();
        return false;
    }
    hairpin->setInitialTrackAndVoiceAssignment(hairpin->track(), false);
    undoStack->commitChanges();
    return true;
}

inline bool addFreeEndSlur(const INotationPtr& notation, int shape, engraving::Note* startNote,
                           const muse::PointF& endOffset)
{
    if (!notation || !notation->elements() || !startNote || !startNote->chord() || !isSlurShape(shape)) {
        return false;
    }
    auto undoStack = notation->undoStack();
    engraving::Score* score = notation->elements()->msScore();
    if (!undoStack || !score || undoStack->isLocked()) {
        return false;
    }

    undoStack->prepareChanges(muse::TranslatableString("undoableAction", "Add slur"));
    engraving::Slur* slur = score->addSlur(startNote->chord(), startNote->chord(), nullptr);
    if (!slur || !slur->frontSegment()) {
        undoStack->rollbackChanges();
        return false;
    }
    if (shape == RepriseToolModel::DashedSlur) {
        slur->setStyleType(engraving::SlurStyleType::Dashed);
    }
    slur->frontSegment()->setSlurOffset(engraving::Grip::END, endOffset);
    undoStack->commitChanges();
    return true;
}

inline bool addNoteShape(const INotationPtr& notation, int shape, engraving::Note* first, engraving::Note* last)
{
    if (!notation || !notation->elements() || !first || !last || first == last || !first->chord() || !last->chord()
        || !isNoteAnchoredShape(shape)) {
        return false;
    }

    // Native spanners are stored in musical order even when the gesture was dragged backwards.
    engraving::Note* startNote = first;
    engraving::Note* endNote = last;
    if (endNote->tick() < startNote->tick()) {
        std::swap(startNote, endNote);
    }
    if (startNote->tick() == endNote->tick()) {
        return false;
    }

    auto undoStack = notation->undoStack();
    engraving::Score* score = notation->elements()->msScore();
    if (!undoStack || !score || undoStack->isLocked()) {
        return false;
    }

    if (isSlurShape(shape)) {
        undoStack->prepareChanges(muse::TranslatableString("undoableAction", "Add slur"));
        engraving::Slur* slur = score->addSlur(startNote->chord(), endNote->chord(), nullptr);
        if (!slur) {
            undoStack->rollbackChanges();
            return false;
        }
        if (shape == RepriseToolModel::DashedSlur) {
            slur->setStyleType(engraving::SlurStyleType::Dashed);
        }
        undoStack->commitChanges();
        return true;
    }

    if (shape == RepriseToolModel::GuitarBend) {
        // MuseScore owns bend validation, fret adaptation, playback semantics and linked-note handling.
        undoStack->prepareChanges(muse::TranslatableString("undoableAction", "Add guitar bend"));
        engraving::GuitarBend* bend = score->addGuitarBend(engraving::GuitarBendType::BEND, startNote, endNote);
        if (!bend) {
            undoStack->rollbackChanges();
            return false;
        }
        undoStack->commitChanges();
        return true;
    }

    undoStack->prepareChanges(muse::TranslatableString("undoableAction", "Add glissando"));
    engraving::Glissando* gliss = engraving::Factory::createGlissando(startNote);
    if (!gliss) {
        undoStack->rollbackChanges();
        return false;
    }
    gliss->setAnchor(engraving::Spanner::Anchor::NOTE);
    gliss->setStartElement(startNote);
    gliss->setEndElement(endNote);
    gliss->setTick(startNote->tick());
    gliss->setTick2(endNote->tick());
    gliss->setTrack(startNote->track());
    gliss->setTrack2(endNote->track());
    gliss->setParent(startNote);

    if (shape == RepriseToolModel::TabSlide) {
        // A tab slide is the straight, textless, pitch-shifted native glissando variant.
        gliss->setGlissandoType(engraving::GlissandoType::STRAIGHT);
        gliss->setPropertyFlags(engraving::Pid::GLISS_TYPE, engraving::PropertyFlags::UNSTYLED);
        gliss->setText(u"");
        gliss->setShowText(false);
        gliss->setGlissandoShift(true);
    }
    // A regular glissando intentionally keeps the score's native MuseScore glissando style.

    score->undoAddElement(gliss);
    undoStack->commitChanges();
    return true;
}

inline bool addTimeShape(const INotationPtr& notation, int shape, engraving::staff_idx_t staffIdx,
                         engraving::track_idx_t sourceTrack, const engraving::Fraction& startTick,
                         const engraving::Fraction& endTick)
{
    if (!notation || !notation->elements() || endTick <= startTick
        || isNoteAnchoredShape(shape) || isHairpinShape(shape)) {
        return false;
    }
    auto undoStack = notation->undoStack();
    engraving::Score* score = notation->elements()->msScore();
    if (!undoStack || !score || undoStack->isLocked()) {
        return false;
    }

    engraving::Segment* startSegment = score->tick2segmentMM(startTick, true);
    engraving::Segment* endSegment = score->tick2segmentMM(endTick, true);
    if (!startSegment || !endSegment) {
        return false;
    }

    engraving::Spanner* spanner = nullptr;
    switch (shape) {
    case RepriseToolModel::Ottava8va: {
        engraving::Ottava* ottava = engraving::Factory::createOttava(score->dummy());
        ottava->setOttavaType(engraving::OttavaType::OTTAVA_8VA);
        spanner = ottava;
        break;
    }
    case RepriseToolModel::Ottava15ma: {
        engraving::Ottava* ottava = engraving::Factory::createOttava(score->dummy());
        ottava->setOttavaType(engraving::OttavaType::OTTAVA_15MA);
        spanner = ottava;
        break;
    }
    case RepriseToolModel::Trill: {
        engraving::Trill* trill = engraving::Factory::createTrill(score->dummy());
        // Explicitly setting the type creates MuseScore's initial trill ornament.
        trill->setTrillType(engraving::TrillType::TRILL_LINE);
        spanner = trill;
        break;
    }
    case RepriseToolModel::TrillExtension: {
        // A freshly-created Trill defaults to TRILL_LINE but has no initial Ornament yet.
        // That makes it the native MuseScore equivalent of Finale's pure wiggle extension.
        spanner = engraving::Factory::createTrill(score->dummy());
        break;
    }
    case RepriseToolModel::BracketBoth:
    case RepriseToolModel::BracketBothDashed:
    case RepriseToolModel::BracketEnd:
    case RepriseToolModel::BracketEndDashed:
    case RepriseToolModel::Line:
    case RepriseToolModel::DashedLine: {
        engraving::TextLine* line = engraving::Factory::createTextLine(score->dummy());
        line->setLineVisible(true);
        line->setLineStyle(isDashedShape(shape) ? engraving::LineType::DASHED : engraving::LineType::SOLID);
        if (isDashedShape(shape)) {
            // LINE_STYLE is styled by default; make this preset explicit so it survives save/reopen.
            line->setPropertyFlags(engraving::Pid::LINE_STYLE, engraving::PropertyFlags::UNSTYLED);
        }
        const bool beginHook = shape == RepriseToolModel::BracketBoth || shape == RepriseToolModel::BracketBothDashed;
        const bool endHook = beginHook || shape == RepriseToolModel::BracketEnd || shape == RepriseToolModel::BracketEndDashed;
        line->setBeginHookType(beginHook ? engraving::HookType::HOOK_90 : engraving::HookType::NONE);
        line->setEndHookType(endHook ? engraving::HookType::HOOK_90 : engraving::HookType::NONE);
        spanner = line;
        break;
    }
    default:
        return false;
    }

    if (!spanner) {
        return false;
    }

    undoStack->prepareChanges(muse::TranslatableString("undoableAction", "Add line"));
    score->cmdAddSpanner(spanner, staffIdx, startSegment, endSegment, false);
    if (spanner->hasVoiceAssignmentProperties()) {
        spanner->setInitialTrackAndVoiceAssignment(sourceTrack, false);
    }
    undoStack->commitChanges();
    return true;
}

} // namespace mu::notation::reprise::smartshape
