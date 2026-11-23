// Standalone checks for the Reprise interaction contract. No Qt or score needed.
#include "src/notationscene/reprise/reprisetoolstate.h"

#include <cstdlib>
#include <iostream>

using namespace mu::notation::reprise;

namespace {
int checks = 0;
void require(bool condition, const char* message)
{
    ++checks;
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        std::exit(EXIT_FAILURE);
    }
}
}

int main()
{
    ToolState state;
    require(state.activeTool() == Tool::Selection && !state.operationActive(), "initial neutral state");
    require(!state.beginOperation(), "Selection delegates operations to MuseScore");
    require(state.escape(false) == EscapeResult::DelegateToNative, "neutral Escape delegates");

    const Tool tools[] = { Tool::NoteEntry, Tool::SmartShape, Tool::Expression, Tool::Staff, Tool::Repitch,
                           Tool::Articulation, Tool::TimeSignature, Tool::Clef, Tool::KeySignature, Tool::Repeat, Tool::Measure };
    for (Tool tool : tools) {
        state.activateTool(tool);
        require(state.activeTool() == tool && !state.operationActive(), "tool activation starts idle");
        require(state.escape(true) == EscapeResult::DelegateToNative && state.activeTool() == tool,
                "native Escape ownership preserves idle tool");
        require(state.beginOperation(), "tool can begin operation");
        require(!state.beginOperation() && state.operationActive(), "nested operation rejected");
        require(state.escape(true) == EscapeResult::DelegateToNative && state.operationActive()
                && state.activeTool() == tool, "native Escape ownership preserves active operation");
        require(state.escape(false) == EscapeResult::OperationCancelled && !state.operationActive()
                && state.activeTool() == tool, "first Escape cancels only operation");
        require(state.escape(false) == EscapeResult::SelectionActivated && state.activeTool() == Tool::Selection
                && !state.operationActive(), "second Escape returns to Selection");
        require(state.escape(false) == EscapeResult::DelegateToNative, "next Escape follows native selection behavior");
    }

    state.activateTool(Tool::SmartShape);
    require(state.beginOperation(), "start operation before switching tool");
    state.activateTool(Tool::Expression);
    require(!state.operationActive() && state.activeTool() == Tool::Expression, "switching tool cancels previous operation");
    require(state.beginOperation(), "start operation before finishing");
    state.finishOperation();
    require(!state.operationActive() && state.activeTool() == Tool::Expression, "finish preserves tool");
    state.finishOperation();
    require(!state.operationActive(), "finish is idempotent");
    require(state.beginOperation(), "start operation before reactivation");
    state.activateTool(Tool::Expression);
    require(!state.operationActive(), "explicit reactivation clears operation");
    require(state.beginOperation(), "start operation before reset");
    state.reset();
    require(state.activeTool() == Tool::Selection && !state.operationActive(), "score/view boundary resets transient state");
    state.reset();
    require(state.activeTool() == Tool::Selection && !state.operationActive(), "reset is idempotent");
    std::cout << "PASS: " << checks << " Reprise ToolState contract checks\n";
}
