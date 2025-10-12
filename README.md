# reactive-state-machine

SCXML 기반 C++ 상태 머신 라이브러리로, W3C SCXML 명세를 따르는 파서와 QuickJS 기반 스크립팅 엔진을 제공합니다. **SCXML과 C++ 코드 간의 양방향 연동**을 통해 상태 머신 로직과 비즈니스 로직을 효과적으로 분리할 수 있습니다.

## 핵심 특징

### 🔄 SCXML ↔ C++ 양방향 연동
- **SCXML → C++**: 상태 전환 시 C++ 함수 자동 호출
- **C++ → SCXML**: Guard 조건을 C++ 함수로 실시간 평가
- **런타임 바인딩**: 실행 시점에 C++ 객체와 SCXML 연결

### 🔧 기술적 구성 요소
- **SCXML 파서**: W3C SCXML 표준 준수 파싱 엔진 (libxml++ 기반)
- **QuickJS 엔진**: JavaScript 표현식 평가 및 C++ 함수 바인딩
- **코드 생성기**: SCXML에서 C++ 상태 머신 클래스 자동 생성
- **런타임 엔진**: 이벤트 처리 및 상태 전환 실행

## 프로젝트 구조

```
reactive-state-machine/
├── rsm/                           # 핵심 라이브러리
│   ├── include/                   # 헤더 파일
│   │   ├── parsing/              # SCXML 파싱 관련
│   │   ├── model/                # 메모리 모델 (파싱 결과)
│   │   ├── runtime/              # 상태 머신 실행 엔진
│   │   ├── scripting/            # QuickJS 스크립팅 엔진
│   │   └── common/               # 공통 유틸리티
│   └── src/                      # 구현 파일
│
├── tools/codegen/                # 코드 생성기 도구
│   └── main.cpp                  # SCXML → C++ 코드 생성
│
├── tests/                        # 테스트 스위트
│   ├── engine/                   # 엔진 기능 테스트
│   ├── core/                     # 파서 테스트
│   └── integration/              # 통합 테스트
│
└── third_party/                  # 서드파티 라이브러리
    ├── quickjs/                  # JavaScript 엔진
    ├── libxml++/                 # XML 파싱
    ├── spdlog/                   # 로깅
    └── cpp-httplib/              # HTTP 통신
```

## 구현 현황

### ✅ 완료된 기능
- **SCXML 파서**: W3C 표준 준수, XInclude 지원
- **메모리 모델**: 파싱된 SCXML의 객체 모델
- **QuickJS 엔진**: JavaScript 표현식 평가, SCXML 시스템 변수 지원
- **테스트 프레임워크**: GoogleTest 기반 단위/통합 테스트

### 🚧 진행중인 기능
- **코드 생성기**: 기본 구조 완료, 실제 SCXML 기반 생성 로직 구현중
- **C++ 바인딩 시스템**: QuickJS와 C++ 함수 연동

### 📋 계획된 기능
- **완전한 C++ 콜백 지원**: Action 및 Guard 함수의 완전한 바인딩
- **전체 SCXML 명세**: 병렬 상태, 이력 상태, 데이터 모델 완전 구현
- **빌드 도구 통합**: CMake 패키지, pkg-config 지원

## 빌드 및 설치

### 요구사항
- CMake 3.14+
- C++20 호환 컴파일러 (GCC 10+, Clang 12+, MSVC 2019+)
- 서드파티 의존성들은 submodule로 포함됨

### 빌드 방법

```bash
# 기본 빌드
./build.sh

# 테스트 포함 빌드
./build.sh Debug ON

# 릴리스 빌드
./build.sh Release
```

### 테스트 실행

```bash
cd build
ctest                                    # 모든 테스트
ctest -R "engine"                       # 엔진 테스트만
ctest -R "SimpleSCXMLTest"              # QuickJS 엔진 테스트
```

## 사용 예시

### 1. SCXML 파일 작성 (C++ 콜백 연동)
```xml
<?xml version="1.0" encoding="UTF-8"?>
<scxml xmlns="http://www.w3.org/2005/07/scxml" version="1.0" initial="idle">
  <datamodel>
    <data id="temperature" expr="25"/>
  </datamodel>

  <state id="idle">
    <!-- C++ Guard 함수로 조건 평가 -->
    <transition event="temp_change"
               cond="hardware.isTemperatureHigh()"
               target="cooling"/>
  </state>

  <state id="cooling">
    <onentry>
      <!-- C++ Action 함수 호출 -->
      <script>hardware.startAirConditioner()</script>
      <script>hardware.logEvent("Cooling started")</script>
    </onentry>

    <onexit>
      <script>hardware.stopAirConditioner()</script>
    </onexit>

    <!-- 복합 조건: JS 표현식 + C++ Guard -->
    <transition event="temp_change"
               cond="temperature <= 25 && hardware.isSystemStable()"
               target="idle"/>
  </state>
</scxml>
```

### 2. 코드 생성
```bash
# SCXML에서 C++ 클래스 생성
./build/tools/codegen/scxml-codegen -o thermostat.cpp thermostat.scxml
```

### 3. C++ 콜백 클래스 구현
```cpp
#include "thermostat.cpp"  // 생성된 파일

// C++ 콜백 클래스 - SCXML에서 호출될 함수들 정의
class HardwareController {
public:
    // Guard 함수들 (조건 평가용)
    bool isTemperatureHigh() {
        double temp = sensor.getCurrentTemperature();
        return temp > 30.0;
    }

    bool isSystemStable() {
        return !system.hasErrors() && system.getUptime() > 60;
    }

    // Action 함수들 (상태 변화 시 실행)
    void startAirConditioner() {
        aircon.setPower(true);
        aircon.setTargetTemp(22.0);
        logger.info("Air conditioner started");
    }

    void stopAirConditioner() {
        aircon.setPower(false);
        logger.info("Air conditioner stopped");
    }

    void logEvent(const std::string& message) {
        logger.info("State machine event: " + message);
    }

private:
    TemperatureSensor sensor;
    AirConditioner aircon;
    SystemMonitor system;
    Logger logger;
};

// 메인 애플리케이션
class ThermostatApp {
public:
    ThermostatApp() {
        // C++ 객체를 SCXML에 바인딩
        stateMachine.bindObject("hardware", &hardware);
        stateMachine.start();
    }

    void processTemperatureReading(double temp) {
        stateMachine.setData("temperature", temp);
        stateMachine.processEvent("temp_change");
    }

private:
    ThermostatStateMachine stateMachine;
    HardwareController hardware;
};
```

## 아키텍처: 3가지 동작 모드

RSM은 프로젝트 요구사항에 맞춰 선택 가능한 3가지 모드를 제공합니다:

### 1️⃣ 정적 컴파일 모드 (최고 성능)
```cpp
// SCXML → 순수 C++ 코드 생성
#include "Thermostat_sm.h"  // 자동 생성된 헤더

using namespace RSM::Generated;

// 템플릿 기반 구현 (가상 함수 없음, 완전한 인라인 가능)
class ThermostatLogic : public ThermostatBase<ThermostatLogic> {
public:
    // Guard 함수
    bool isHot() {
        return sensor.read() > 25.0;
    }

    // Action 함수
    void startCooling() {
        fan.start();
    }

    void stopCooling() {
        fan.stop();
    }

    // Friend declaration for base class access
    friend class ThermostatBase<ThermostatLogic>;

private:
    Sensor sensor;
    Fan fan;
};

int main() {
    ThermostatLogic thermostat;
    thermostat.initialize();
    thermostat.processEvent(Event::TempChange);  // 완전 인라인 가능, vtable 없음
}
```
- **성능**: 가상 함수 오버헤드 제로, 컴파일 타임 최적화
- **메모리**: ~8 bytes (상태 변수만), vtable 없음
- **용도**: 임베디드, 실시간 시스템, 고성능 애플리케이션

### 2️⃣ 동적 인터프리터 모드 (최대 유연성)
```cpp
// 런타임 SCXML 로딩
RSM::StateMachine sm("thermostat.scxml");
sm.registerGlobalFunction("isHot", []() {
    return sensor.read() > 25.0;
});
sm.start();
```
- **유연성**: SCXML 수정 후 재컴파일 불필요
- **디버깅**: 풍부한 런타임 정보
- **용도**: 서버, 개발/테스트 환경

### 3️⃣ 하이브리드 모드 (균형)
```cpp
// 안전 크리티컬: 정적 컴파일
SafetyControllerSM safetySM;

// 비즈니스 로직: 동적 인터프리터
RSM::StateMachine businessSM("workflow.scxml");
```
- **용도**: 복잡한 시스템에서 성능과 유연성 균형

## 사용법

### 빠른 시작 (CMake 자동 코드 생성)
```cmake
# 실행 파일 생성
add_executable(my_app main.cpp)

# SCXML에서 자동으로 C++ 코드 생성 및 통합
rsm_add_state_machine(
    TARGET my_app
    SCXML_FILE thermostat.scxml
)

# RSM 라이브러리 링크
target_link_libraries(my_app PRIVATE rsm_unified)
target_include_directories(my_app PRIVATE ${CMAKE_SOURCE_DIR}/rsm/include)
```
이 CMake 함수는:
- SCXML 파일이 변경되면 자동으로 C++ 코드 재생성
- 생성된 헤더를 타겟에 자동 추가
- 의존성 추적으로 빌드 최적화

### SCXML 작성
```xml
<scxml name="Thermostat" initial="idle">
  <state id="idle">
    <transition event="check" cond="isHot()" target="cooling">
      <script>startCooling()</script>
    </transition>
  </state>

  <state id="cooling">
    <transition event="check" cond="!isHot()" target="idle"/>
  </state>
</scxml>
```

### 비즈니스 로직 구현
```cpp
#include "Thermostat_sm.h"  // 자동 생성된 헤더

using namespace RSM::Generated;

// 템플릿 기반 비즈니스 로직 구현
class ThermostatLogic : public ThermostatBase<ThermostatLogic> {
public:
    // Guard 함수 - 조건 평가
    bool isHot() {
        return sensor_.read() > threshold_;
    }

    // Action 함수 - 상태 전환 시 실행
    void startCooling() {
        fan_.start();
        metrics_.record("cooling_started");
    }

    void stopCooling() {
        fan_.stop();
        metrics_.record("cooling_stopped");
    }

    // Entry/Exit 액션
    void onEnterIdle() {
        LOG_INFO("Entered idle state");
    }

    // Friend declaration for base class access (required)
    friend class ThermostatBase<ThermostatLogic>;

private:
    Sensor sensor_;
    Fan fan_;
    MetricsCollector metrics_;
    double threshold_ = 25.0;
};

// 사용 예
int main() {
    ThermostatLogic thermostat;
    thermostat.initialize();  // 초기 상태로 이동

    // 이벤트 처리
    thermostat.processEvent(Event::Check);

    // 현재 상태 확인
    if (thermostat.getCurrentState() == State::Cooling) {
        // 쿨링 중
    }
}
```

## 핵심 설계 원칙

### 📐 템플릿 기반 제로 오버헤드 설계
- **제로 오버헤드**: 가상 함수 없음, 완전한 컴파일 타임 다형성
- **완전 인라인 가능**: 모든 메서드 호출이 인라인으로 최적화
- **타입 안정성**: 컴파일 타임에 모든 메서드 존재 검증
- **코드 분리**: 생성 코드(Base)와 사용자 코드(Derived) 명확한 분리

*기술적으로는 CRTP(Curiously Recurring Template Pattern)를 사용하지만, 사용자는 단순히 생성된 베이스 클래스를 상속하기만 하면 됩니다.*

### 🎯 Convention over Configuration
- **자동 의존성 추적**: SCXML 변경 시 자동 재생성
- **스마트 네이밍**: SCXML name → C++ 클래스명 자동 변환
- **CMake 자동화**: rsm_add_state_machine() 한 줄로 통합

### ⚡ 제로 오버헤드 추상화
- **vtable 없음**: 템플릿 기반으로 가상 함수 오버헤드 제거
- **완전 인라인**: 컴파일러가 모든 호출을 인라인 최적화 가능
- **최소 메모리**: 상태 변수만 저장 (~8 bytes)

## 개발 방향성

### "Reactive"의 핵심: SCXML ↔ C++ 양방향 연동

이 프로젝트에서 "Reactive"는 **SCXML과 C++ 코드가 서로 반응하며 상호작용**하는 시스템을 의미합니다:

#### 🔄 양방향 호출 구조
```
SCXML (상태 머신 로직) ←→ C++ (비즈니스 로직)
     ↓                    ↑
  Action 실행         Guard 조건 평가
```

#### 💡 실제 동작 예시
```xml
<!-- SCXML에서 C++ 함수 직접 호출 -->
<transition cond="hardware.isTemperatureHigh()" target="cooling">
  <script>hardware.startAirConditioner()</script>
</transition>
```

```cpp
// C++ 클래스 정의
class HardwareController {
public:
    bool isTemperatureHigh() { return sensor.getTemp() > 30; }
    void startAirConditioner() { aircon.start(); }
};

// SCXML에 바인딩
stateMachine.bindObject("hardware", &controller);
```

#### 🎯 장점
- **실시간 반응**: 센서 값, 시스템 상태 등 실시간 데이터로 조건 평가
- **관심사 분리**: 상태 로직(SCXML) vs 비즈니스 로직(C++) 명확한 구분
- **재사용성**: 동일한 C++ 로직을 여러 상태 머신에서 활용
- **테스트 용이성**: Mock 객체로 SCXML과 C++ 로직을 독립적으로 테스트

## 🚧 현재 진행 상황

### ✅ 완료된 기능
- **W3C SCXML 1.0 완전 준수** (202/202 테스트 통과)
- **C++ 함수 바인딩**: SCXML에서 C++ 함수 직접 호출 가능
- **정적 코드 생성기 프로토타입**: TDD 방식으로 개발 중
  - State/Event enum 생성
  - 기본 클래스 구조 생성
  - 6개 테스트 케이스 모두 통과

### ✅ 최근 완료 (2025-10-12)
- **정적 코드 생성기 완성** (템플릿 기반 제로 오버헤드)
  - [x] SCXML 파서 통합
  - [x] State/Event enum 자동 생성
  - [x] Template-based Base 클래스 생성
  - [x] Guard 조건 메서드 시그니처 생성
  - [x] Action 메서드 (entry/exit/transition) 생성
  - [x] processEvent() 로직 자동 생성
- **CLI 도구 구현** (`scxml-codegen`)
  - SCXML 파일 → C++ 헤더 생성
  - 사용자 가이드 메시지 출력
- **CMake 통합 완성**
  - `rsm_add_state_machine()` 함수 구현
  - 자동 의존성 추적 및 재생성
  - 예제 프로젝트 포함
- **통합 테스트 완료**
  - 실제 SCXML → 생성 → 컴파일 → 실행 검증
  - 7개 테스트 케이스 모두 통과

### 🎯 개발 목표: W3C SCXML 전체 사양 만족

**목표**: 정적 코드 생성기가 W3C SCXML 1.0의 모든 사양을 만족하는 C++ 코드를 생성하도록 개선

현재 동적 런타임은 202/202 W3C 테스트를 통과하지만, 정적 코드 생성기는 기본 기능만 지원합니다 (~25-30% 커버리지).
**하이브리드 아키텍처**를 통해 모든 SCXML 기능을 지원하면서도 "사용하지 않는 것에 대해 비용을 지불하지 않는" C++ 철학을 유지합니다.

#### 아키텍처 원칙: Zero-Overhead by Default

**Tier 0: Zero Overhead** (항상 무료 - ~80% 기능)
- Atomic, Compound, Final states
- Event-based transitions with guards
- Entry/Exit handlers, transition actions
- Eventless transitions
- In() predicate
- LCA calculation (compile-time precomputed)
- Done events

**Tier 1: Minimal Overhead** (필요시 최소 비용 - ~15% 기능)
- Parallel states: `std::set<State>` or `std::bitset<N>`
- History states: State variable (1 byte) or history stack (~24 bytes)
- Internal event queue: `std::queue<Event>` (only if `<raise>` used)

**Tier 2: Conditional Overhead** (사용시에만 추가 - ~5% 기능)
- JavaScript engine: QuickJS (~200KB, only if complex JS expressions detected)
- HTTP client: cpp-httplib (~50KB, only if `<invoke type="http">`)
- Timer system: (~1KB, only if `<send delay>` used)

#### 구현 로드맵 (3-5주)

**Week 1: Feature Detection System**
- SCXML 분석기 구현: 사용된 기능 자동 탐지
- `SCXMLFeatures` 구조체: 필요한 Tier별 기능 결정
- Policy 선택 로직: Tier 2 기능 조건부 활성화

**Week 2-3: Structural Features (Tier 0-1)**
- Compound states: 계층 구조 인코딩, LCA 사전 계산
- Parallel states: 다중 활성 상태 관리
- History states: Shallow/Deep history 지원
- Final states: Done event 자동 생성
- Internal transitions: Exit/Entry 생략 로직

**Week 4: Dynamic Features (Tier 2, Optional)**
- JavaScript Policy: NoJavaScript (zero-size) vs WithJavaScript (QuickJS)
- Timer Policy: NoTimer vs WithTimer
- Invoke Policy: NoInvoke vs WithHTTP vs WithChildMachine
- Policy-based code generation templates

**Week 5: W3C Test Validation**
- 202개 W3C 테스트 자동화
- 각 테스트: SCXML → Generate → Compile → Run → Verify
- 동적 런타임과 동일한 동작 검증
- 목표: 202/202 PASSED with generated code

#### 기대 효과

**성능**: 정적 생성 코드가 동적 런타임보다 100배 빠름 (기존 벤치마크)
**메모리**: 단순 머신 ~8 bytes vs 동적 런타임 ~100KB
**표준 준수**: W3C SCXML 1.0 완전 호환 (100% 커버리지)
**유연성**: 필요한 기능만 선택적으로 추가 가능

자세한 아키텍처 설계는 [ARCHITECTURE.md](ARCHITECTURE.md) 참조

### 📊 테스트 현황
```
Static Codegen Tests:     6/6 PASSED ✅
Integration Tests:        7/7 PASSED ✅ (SCXML → Generated C++ → Compilation → Execution)
W3C SCXML Compliance:   202/202 PASSED ✅
Unit Tests:             All PASSED ✅
```

## 기여하기

1. 이슈 생성 또는 기존 이슈 확인
2. 포크 및 브랜치 생성
3. 코드 작성 및 테스트 추가
4. clang-format으로 코드 스타일 정리
5. 풀 리퀘스트 제출

### 개발 환경 설정
```bash
# Git hooks 설정 (자동 포맷팅)
./setup-hooks.sh

# 수동 포맷팅
find rsm -name '*.cpp' -o -name '*.h' | xargs clang-format -i
```

## 라이선스

MIT License - 자세한 내용은 [LICENSE](LICENSE) 파일 참조
