#pragma once

namespace RSM {

enum class Type {
    ATOMIC,    // 자식 상태가 없는 상태
    COMPOUND,  // 자식 상태가 있는 상태
    PARALLEL,  // 병렬 상태
    FINAL,     // 종료 상태
    HISTORY,   // 히스토리 의사 상태
    INITIAL    // 초기 의사 상태
};

enum class HistoryType {
    NONE,     // 히스토리 아님
    SHALLOW,  // Shallow 히스토리
    DEEP      // Deep 히스토리
};

}  // namespace RSM