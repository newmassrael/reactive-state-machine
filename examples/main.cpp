#include "MyStateMachine.h"
#include <iostream>
#include <thread>
#include <chrono>

int main()
{
    // 상태 머신 팩토리를 통해 인스턴스 생성
    MyStateMachineFactory factory;
    auto stateMachineInterface = factory.createStateMachine();

    // 인터페이스를 통한 상태 머신 제어
    std::cout << "Starting state machine..." << std::endl;
    stateMachineInterface->start();

    // MyStateMachine으로 다운캐스트 (직접 컨텍스트 접근을 위해)
    // 참고: 실제 제품 코드에서는 의존성 주입 사용 권장
    auto &stateMachine = static_cast<MyStateMachine &>(*stateMachineInterface);

    // 시나리오 1: 이벤트 기반 전환 테스트
    std::cout << "\n=== Scenario 1: Event-based transition ===" << std::endl;
    std::cout << "Current state: " << stateMachine.getCurrentState() << std::endl;
    std::cout << "Firing Event1..." << std::endl;
    stateMachine.fireEvent1();

    // 상태 전환 기다리기
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    std::cout << "New state: " << stateMachine.getCurrentState() << std::endl;

    // 시나리오 2: 반응형 가드 조건 테스트
    std::cout << "\n=== Scenario 2: Reactive guard condition ===" << std::endl;

    // Test2Sub1 상태에 있을 것으로 예상
    std::cout << "Current state: " << stateMachine.getCurrentState() << std::endl;

    std::cout << "Setting currentUser to 'admin'..." << std::endl;
    stateMachine.getContext().currentUser.set("admin");

    // 아직 isActive가 false이므로 전환 안됨
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    std::cout << "State after setting user: " << stateMachine.getCurrentState() << std::endl;

    std::cout << "Setting isActive to true..." << std::endl;
    stateMachine.getContext().isActive.set(true);

    // 반응형 가드에 의한 전환 발생
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    std::cout << "State after setting isActive: " << stateMachine.getCurrentState() << std::endl;

    // 시나리오 3: 타이머 이벤트 테스트
    std::cout << "\n=== Scenario 3: Timer event (5 second delay) ===" << std::endl;
    std::cout << "Current state: " << stateMachine.getCurrentState() << std::endl;

    // Test3Sub1으로 이동 후 타이머 시작
    // Test3 상태로의 자동 전환은 StateMachineImpl에서 처리

    // 타이머 완료 기다리기
    std::cout << "Waiting for timer..." << std::endl;
    std::this_thread::sleep_for(std::chrono::seconds(6));
    std::cout << "State after timer: " << stateMachine.getCurrentState() << std::endl;

    // 시나리오 4: 오류 이벤트 테스트
    std::cout << "\n=== Scenario 4: Error event ===" << std::endl;
    std::cout << "Current state: " << stateMachine.getCurrentState() << std::endl;

    // 오류 컨텍스트 생성
    EventContext errorContext;
    errorContext.setValue("errorCode", 404);
    errorContext.setValue("errorMessage", std::string("Resource not found"));

    std::cout << "Firing error event..." << std::endl;
    stateMachine.fireErrorEvent("not_found", errorContext);

    // 오류 처리 기다리기
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    std::cout << "State after error: " << stateMachine.getCurrentState() << std::endl;

    // 상태 머신 중지
    std::cout << "\nStopping state machine..." << std::endl;
    stateMachine.stop();

    return 0;
}
