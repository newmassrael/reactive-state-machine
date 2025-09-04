#pragma once

#include "common/Logger.h"
#include <chrono>
#include <future>
#include <thread>

namespace SCXML {
namespace Common {

/**
 * @brief Timeout 기반 graceful 스레드 join 유틸리티
 */
class GracefulJoin {
public:
    /**
     * @brief 스레드를 timeout과 함께 gracefully join
     * @param thread join할 스레드
     * @param timeoutSeconds 타임아웃 시간 (초)
     * @param threadName 로그용 스레드 이름
     * @return true if joined successfully, false if timed out
     */
    static bool joinWithTimeout(std::thread &thread, int timeoutSeconds = 5, const std::string &threadName = "Thread") {
        if (!thread.joinable()) {
            return true;
        }

        try {
            // 별도 스레드에서 join 실행
            auto future = std::async(std::launch::async, [&thread]() {
                thread.join();
                return true;
            });

            // 타임아웃 대기
            auto status = future.wait_for(std::chrono::seconds(timeoutSeconds));

            if (status == std::future_status::ready) {
                // 성공적으로 join됨
                return true;
            } else {
                // 타임아웃 발생
                Logger::warning("GracefulJoin: " + threadName + " join timeout (" + std::to_string(timeoutSeconds) +
                                "s), detaching thread");

                if (thread.joinable()) {
                    thread.detach();
                }
                return false;
            }
        } catch (const std::exception &e) {
            Logger::error("GracefulJoin: Exception during " + threadName + " join: " + e.what());
            if (thread.joinable()) {
                thread.detach();
            }
            return false;
        }
    }

    /**
     * @brief 여러 스레드를 순차적으로 graceful join
     * @param threads join할 스레드들
     * @param timeoutSeconds 각 스레드별 타임아웃
     * @param threadBaseName 로그용 기본 스레드 이름
     * @return 성공적으로 join된 스레드 수
     */
    template <typename Container>
    static size_t joinMultiple(Container &threads, int timeoutSeconds = 5,
                               const std::string &threadBaseName = "Thread") {
        size_t successCount = 0;
        size_t index = 0;

        for (auto &thread : threads) {
            std::string threadName = threadBaseName + "_" + std::to_string(index++);
            if (joinWithTimeout(thread, timeoutSeconds, threadName)) {
                successCount++;
            }
        }

        return successCount;
    }

    /**
     * @brief 조건부 join - 스레드가 실제로 실행 중인지 체크하며 join
     * @param thread join할 스레드
     * @param shouldStop 종료 신호 플래그 (atomic<bool>*)
     * @param timeoutSeconds 총 대기 시간
     * @param threadName 로그용 스레드 이름
     * @return true if joined successfully
     */
    static bool conditionalJoin(std::thread &thread, std::atomic<bool> *shouldStop, int timeoutSeconds = 5,
                                const std::string &threadName = "Thread") {
        if (!thread.joinable()) {
            return true;
        }

        // 종료 신호 보내기
        if (shouldStop) {
            *shouldStop = true;
        }

        // 짧은 간격으로 상태 체크
        const int checkIntervalMs = 100;
        const int maxChecks = (timeoutSeconds * 1000) / checkIntervalMs;

        for (int i = 0; i < maxChecks; ++i) {
            std::this_thread::sleep_for(std::chrono::milliseconds(checkIntervalMs));

            // 스레드가 자연스럽게 종료되었는지 체크
            if (!thread.joinable()) {
                return true;
            }
        }

        // 여전히 실행 중이면 강제 detach
        Logger::warning("GracefulJoin: " + threadName + " didn't terminate gracefully, detaching");
        if (thread.joinable()) {
            thread.detach();
        }

        return false;
    }
};

}  // namespace Common
}  // namespace SCXML
