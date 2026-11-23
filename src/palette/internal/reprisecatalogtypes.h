// SPDX-License-Identifier: GPL-3.0-only
// MuseScore-Studio-CLA-applies
#pragma once

#include <array>
#include "palette.h"

namespace mu::palette::reprise {
struct CatalogType {
    const char* id;
    Palette::Type type;
};

// Stable identifiers are independent of translated labels and model row order.
inline constexpr std::array<CatalogType, 29> catalogTypes {{
    { "clefs", Palette::Type::Clef },
    { "key-signatures", Palette::Type::KeySig },
    { "time-signatures", Palette::Type::TimeSig },
    { "tempo", Palette::Type::Tempo },
    { "pitch", Palette::Type::Pitch },
    { "accidentals", Palette::Type::Accidental },
    { "dynamics", Palette::Type::Dynamic },
    { "articulations", Palette::Type::Articulation },
    { "text", Palette::Type::Text },
    { "keyboard", Palette::Type::Keyboard },
    { "repeats-jumps", Palette::Type::Repeat },
    { "barlines", Palette::Type::BarLine },
    { "layout", Palette::Type::Layout },
    { "brackets", Palette::Type::Bracket },
    { "ornaments", Palette::Type::Ornament },
    { "breaths-pauses", Palette::Type::Breath },
    { "grace-notes", Palette::Type::GraceNote },
    { "noteheads", Palette::Type::NoteHead },
    { "arpeggios-glissandos", Palette::Type::Arpeggio },
    { "tremolos", Palette::Type::Tremolo },
    { "harp", Palette::Type::Harp },
    { "guitar", Palette::Type::Guitar },
    { "fingerings", Palette::Type::Fingering },
    { "fretboard-diagrams", Palette::Type::FretboardDiagram },
    { "accordion", Palette::Type::Accordion },
    { "bagpipe-embellishments", Palette::Type::BagpipeEmbellishment },
    { "handbells", Palette::Type::Handbells },
    { "beam-properties", Palette::Type::Beam },
    { "lines", Palette::Type::Line },
}};
}
