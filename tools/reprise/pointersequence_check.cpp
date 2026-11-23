// Contract checks for cancellation of an owned press/release sequence.
#include "notationscene/reprise/reprisepointersequence.h"
#include <cstdlib>
#include <iostream>

using mu::notation::reprise::PointerSequence;
int checks = 0;
void require(bool value, const char* label)
{
    ++checks;
    if (!value) {
        std::cerr << "FAIL: " << label << '\n';
        std::exit(EXIT_FAILURE);
    }
}
int main()
{
    PointerSequence sequence;
    require(!sequence.ownsPointer() && !sequence.pending(), "initial native ownership");
    auto release = sequence.release();
    require(!release.consumed && !release.commitCandidate, "unmatched release delegates");
    require(sequence.begin(), "valid press takes ownership");
    require(sequence.ownsPointer() && sequence.pending(), "owned press is pending");
    require(!sequence.begin() && sequence.pending(), "nested press is rejected");
    sequence.cancel();
    require(sequence.ownsPointer() && !sequence.pending(), "Esc preserves release ownership");
    sequence.cancel();
    require(sequence.ownsPointer() && !sequence.pending(), "cancel is idempotent");
    release = sequence.release();
    require(release.consumed && !release.commitCandidate, "cancelled release is consumed without commit");
    require(!sequence.ownsPointer() && !sequence.pending(), "release resets state");
    release = sequence.release();
    require(!release.consumed && !release.commitCandidate, "no second consumption after cancel");
    require(sequence.begin(), "next gesture starts after cancellation");
    release = sequence.release();
    require(release.consumed && release.commitCandidate, "active release offers a commit candidate");
    require(!sequence.ownsPointer() && !sequence.pending(), "candidate release resets before score work");
    require(sequence.begin(), "press before focus/grab reset");
    sequence.reset();
    release = sequence.release();
    require(!release.consumed && !release.commitCandidate, "ungrab reset does not retain ownership");
    sequence.cancel();
    require(!sequence.ownsPointer() && !sequence.pending(), "cancel while idle is harmless");
    std::cout << "PASS: " << checks << " pointer ownership checks\n";
}
