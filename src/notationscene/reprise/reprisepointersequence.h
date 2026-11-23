/*
 * SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Reprise Notation contributors
 */
#pragma once

namespace mu::notation::reprise {
struct PointerRelease {
    bool consumed;
    bool commitCandidate;
};

// Ownership outlives logical cancellation, so a cancelled press has no native release.
// commitCandidate does not validate endpoints or modify a score.
class PointerSequence
{
public:
    bool ownsPointer() const noexcept { return m_owned; }
    bool pending() const noexcept { return m_pending; }
    bool begin() noexcept
    {
        if (m_owned) {
            return false;
        }
        m_owned = true;
        m_pending = true;
        return true;
    }
    void cancel() noexcept { m_pending = false; }
    PointerRelease release() noexcept
    {
        const PointerRelease result { m_owned, m_owned && m_pending };
        reset();
        return result;
    }
    void reset() noexcept
    {
        m_owned = false;
        m_pending = false;
    }

private:
    bool m_owned = false;
    bool m_pending = false;
};
}
