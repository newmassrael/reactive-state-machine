#pragma once

// RSM Logger를 먼저 포함 - 안전한 로깅 대안 제공
#include "common/Logger.h"

// 표준 iostream 포함 (필요한 다른 기능들을 위해)
#include <iostream>

// 시스템 헤더(GoogleTest, OpenSSL 등)와의 충돌 방지
// 오직 RSM 소스 파일에서만 cout/cerr 금지 매크로 적용
#ifndef GTEST_INCLUDE_GTEST_GTEST_H_  // GoogleTest 헤더 제외
#ifndef _OPENSSL_BIO_H                // OpenSSL BIO 헤더 제외
#ifndef _STDIO_H                      // 표준 C stdio 헤더 제외

// std::cout, std::cerr, std::clog 사용을 컴파일 타임에 완전 금지
#define cout                                                                                                           \
    static_assert(false, "std::cout is forbidden in RSM codebase. "                                                    \
                         "Use LOG_INFO(...) instead. "                                                                 \
                         "Example: LOG_INFO(\"Value: {}\", value);")

#define cerr                                                                                                           \
    static_assert(false, "std::cerr is forbidden in RSM codebase. "                                                    \
                         "Use LOG_ERROR(...) instead. "                                                                \
                         "Example: LOG_ERROR(\"Error: {}\", errorMsg);")

#define clog                                                                                                           \
    static_assert(false, "std::clog is forbidden in RSM codebase. "                                                    \
                         "Use LOG_INFO(...) or LOG_DEBUG(...) instead. "                                               \
                         "Example: LOG_DEBUG(\"Debug info: {}\", debugData);")

#endif  // _STDIO_H
#endif  // _OPENSSL_BIO_H
#endif  // GTEST_INCLUDE_GTEST_GTEST_H_

// printf family도 금지 (일관성을 위해)
// printf family는 시스템 헤더 충돌로 인해 비활성화
// 필요시 개별 소스파일에서 직접 처리
#ifdef RSM_STRICT_NO_PRINTF_DISABLED_DUE_TO_SYSTEM_CONFLICTS
#define printf                                                                                                         \
    static_assert(false, "printf is forbidden in RSM codebase. "                                                       \
                         "Use LOG_INFO(...) instead. "                                                                 \
                         "Example: LOG_INFO(\"Value: {}\", value);")

#define fprintf                                                                                                        \
    static_assert(false, "fprintf is forbidden in RSM codebase. "                                                      \
                         "Use LOG_ERROR(...) for stderr or LOG_INFO(...) instead.")
#endif

// puts, putchar도 OpenSSL 등과 충돌하므로 비활성화
#ifdef RSM_STRICT_NO_PUTS_DISABLED_DUE_TO_SYSTEM_CONFLICTS
#define puts                                                                                                           \
    static_assert(false, "puts is forbidden in RSM codebase. "                                                         \
                         "Use LOG_INFO(...) instead.")

#define putchar                                                                                                        \
    static_assert(false, "putchar is forbidden in RSM codebase. "                                                      \
                         "Use LOG_INFO(...) instead.")
#endif

// 안전한 대안 매크로 제공
#define SAFE_PRINT(...) LOG_INFO(__VA_ARGS__)
#define SAFE_PRINT_ERROR(...) LOG_ERROR(__VA_ARGS__)
#define SAFE_PRINT_WARN(...) LOG_WARN(__VA_ARGS__)
#define SAFE_PRINT_DEBUG(...) LOG_DEBUG(__VA_ARGS__)

// 기존 코드 마이그레이션을 위한 도우미 매크로
#define COUT_TO_LOG_INFO(...) LOG_INFO(__VA_ARGS__)
#define CERR_TO_LOG_ERROR(...) LOG_ERROR(__VA_ARGS__)

// RSM 네임스페이스 내에서 사용할 수 있는 안전한 출력 함수들
namespace RSM {
namespace SafeOutput {

// 디버그 빌드에서만 동작하는 안전한 출력 (릴리즈에서는 무시됨)
template <typename... Args> inline void debugPrint(const std::string &format, Args &&...args) {
#ifndef NDEBUG
    LOG_DEBUG(format, std::forward<Args>(args)...);
#else
    (void)format;       // 컴파일러 경고 방지
    ((void)args, ...);  // fold expression으로 모든 args 무시
#endif
}

// 조건부 출력 (condition이 true일 때만 출력)
template <typename... Args> inline void conditionalPrint(bool condition, const std::string &format, Args &&...args) {
    if (condition) {
        LOG_INFO(format, std::forward<Args>(args)...);
    }
}

// 에러 수준별 출력
template <typename... Args> inline void errorPrint(const std::string &format, Args &&...args) {
    LOG_ERROR(format, std::forward<Args>(args)...);
}

template <typename... Args> inline void warningPrint(const std::string &format, Args &&...args) {
    LOG_WARN(format, std::forward<Args>(args)...);
}

template <typename... Args> inline void infoPrint(const std::string &format, Args &&...args) {
    LOG_INFO(format, std::forward<Args>(args)...);
}

}  // namespace SafeOutput
}  // namespace RSM

// 사용법 예시를 주석으로 제공
/*
사용법 예시:

기존 코드:
    std::cout << "Hello " << name << std::endl;
    std::cerr << "Error: " << error << std::endl;

새로운 코드:
    LOG_INFO("Hello {}", name);
    LOG_ERROR("Error: {}", error);

또는 네임스페이스 함수 사용:
    RSM::SafeOutput::infoPrint("Hello {}", name);
    RSM::SafeOutput::errorPrint("Error: {}", error);
    RSM::SafeOutput::debugPrint("Debug: {}", debugData);  // 디버그 빌드에서만 출력
    RSM::SafeOutput::conditionalPrint(verbose, "Verbose: {}", data);  // 조건부 출력
*/