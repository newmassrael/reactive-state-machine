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