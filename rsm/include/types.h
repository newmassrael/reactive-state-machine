#pragma once

namespace RSM {

enum class Type {
    ATOMIC,    // State with no child states
    COMPOUND,  // State with child states
    PARALLEL,  // Parallel state
    FINAL,     // Final state
    HISTORY,   // History pseudo-state
    INITIAL    // Initial pseudo-state
};

enum class HistoryType {
    NONE,     // Not a history state
    SHALLOW,  // Shallow history
    DEEP      // Deep history
};

}  // namespace RSM
