# reactive-state-machine

SCXML 기반 반응형 상태 머신의 C++ 구현체입니다. 이 프로젝트는 W3C SCXML(State Chart XML) 명세를 확장한 상태 머신 코드 생성기 및 실행 엔진을 제공합니다. 일반적인 상태 머신과 달리, 데이터 변화에 자동으로 반응하는 반응형 컨텍스트 가드 시스템과 의존성 주입 기능을 지원합니다.

## 개요

이 프로젝트는 다음 두 가지 주요 구성 요소로 이루어져 있습니다:

1. **상태 머신 생성기 (Generator)**: SCXML 파일을 파싱하는 모듈로, 현재 파서 부분까지만 구현되어 있습니다. 코드 생성 기능은 아직 구현되지 않았습니다.
2. **상태 머신 예제 (Examples)**: 코드 생성기가 최종적으로 생성해내야 할 목표 코드의 예시입니다. 실제 동작하는 상태 머신 구현체를 포함합니다.

## 핵심 기능 상세 설명

### 컨텍스트 가드 시스템 (Reactive Context Guards)

컨텍스트 가드 시스템은 상태 머신이 "조건에 따라 자동으로 반응"하도록 해주는 스마트한 기능입니다. 일반적인 조건문(if-else)과 달리, 조건에 사용된 데이터가 변경되면 자동으로 조건을 다시 평가하고 필요한 액션을 취합니다.

```cpp
// 반응형 컨텍스트의 간단한 예
struct Context {
    ReactiveProperty<int> temperature;  // 온도가 변경되면 자동으로 알림
    ReactiveProperty<bool> doorOpen;    // 문 상태가 변경되면 자동으로 알림
};

// 반응형 가드 예제
ReactiveGuard airConditionerGuard("temperature > 30");  // 온도가 30도 초과면 true
```

이 시스템의 장점:
1. **자동 재평가**: 관련 데이터가 변경될 때마다 조건이 자동으로 재평가됩니다.
2. **종속성 추적**: 가드 조건에 사용된 데이터 속성을 자동으로 추적합니다.
3. **효율성**: 관련 없는 데이터가 변경되면 가드 조건은 재평가되지 않습니다.
4. **유지보수성**: 데이터 변경에 따른 상태 전환 로직이 한 곳에 집중되어 있어 관리가 용이합니다.

### 의존성 주입 (Dependency Injection, DI)

의존성 주입은 상태 머신 시스템의 구성 요소들을 외부에서 주입할 수 있게 합니다:

```cpp
// 의존성 주입 예제
class MyStateMachine : public StateMachineImpl {
public:
    // 생성자를 통한 의존성 주입
    MyStateMachine(
        std::shared_ptr<ILogger> logger,
        std::shared_ptr<IEventHandler> eventHandler
    ) : StateMachineImpl(logger), eventHandler_(eventHandler) {
        // 가드 조건 등록 (런타임에 주입)
        registerGuard("temperatureGuard", std::make_shared<TemperatureGuard>());
        registerGuard("doorGuard", std::make_shared<DoorGuard>());
    }

private:
    std::shared_ptr<IEventHandler> eventHandler_;
};
```

이 접근 방식의 장점:
1. **유연성**: 구성 요소를 쉽게 교체할 수 있어 다양한 환경에 적응 가능
2. **테스트 용이성**: 실제 구현체 대신 모의(mock) 객체를 주입하여 테스트 가능
3. **느슨한 결합**: 시스템 구성 요소 간 의존성 감소
4. **재사용성**: 구성 요소를 다른 상태 머신에서도 재사용 가능

## 프로젝트 구조

```
reactive-state-machine/
├── CMakeLists.txt              # 메인 CMake 빌드 파일
├── build.sh                    # 빌드 스크립트
├── cmake/                      # CMake 설정 파일
│
├── examples/                   # 생성기가 만들어야 할 목표 코드 예시
│   ├── Context.h               # 반응형 컨텍스트 정의
│   ├── MyStateMachine.h        # 사용자 정의 상태 머신 예시
│   ├── StateMachineImpl.h/cpp  # 상태 머신 구현 예시
│   ├── StateMachineInterface.h # 상태 머신 인터페이스
│   └── main.cpp                # 실행 가능한 예제 코드
│
├── generator/                  # SCXML 파서 및 코드 생성기 (현재 파서만 구현됨)
│   ├── CMakeLists.txt          # 생성기 빌드 파일
│   ├── include/                # 생성기 헤더 파일
│   │   ├── Logger.h            # 로깅 유틸리티 헤더
│   │   ├── factory/            # 노드 팩토리 인터페이스
│   │   ├── impl/               # 모델 구현 클래스
│   │   ├── model/              # 모델 인터페이스
│   │   └── parsing/            # SCXML 파싱 로직
│   └── src/                    # 생성기 소스 코드
│
├── scxml/                      # SCXML 예제 파일
│   ├── Main.scxml              # 메인 상태 머신 정의
│   └── Test2Sub1.xml           # 포함된 서브 상태
│
└── tests/                      # 테스트 코드
```

## 빌드 요구사항

- CMake 3.14 이상
- C++17 호환 컴파일러
- libxml++5.0 (XML 파싱용)
- Boost 라이브러리 (Signals2)
- GoogleTest (테스트 빌드 시)

## 빌드 방법

### 기본 빌드 (파서 및 예제)

```bash
./build.sh
```

이 스크립트는 Debug 모드로 빌드하며, 테스트는 포함하지 않습니다.

### 테스트 포함 빌드

```bash
./build.sh Debug ON
```

### 릴리스 빌드

```bash
./build.sh Release
```

### 릴리스 빌드 및 테스트 활성화

```bash
./build.sh Release ON
```

## 사용 방법

### SCXML 파서 사용 방법

SCXML 파서를 사용하여 SCXML 파일을 파싱하고 메모리 내 모델로 변환할 수 있습니다:

```cpp
#include "parsing/SCXMLParser.h"
#include "factory/NodeFactory.h"
#include <memory>
#include <string>

// 노드 팩토리 생성 (모델 객체를 생성하는 팩토리)
auto factory = std::make_shared<NodeFactory>();

// SCXML 파서 생성
SCXMLParser parser(factory);

// 파일에서 SCXML 로드
auto model = parser.parseFile("path/to/your/statechart.scxml");

// 또는 문자열에서 SCXML 로드
std::string scxmlContent = "<?xml version=\"1.0\"?><scxml ...>";
auto modelFromString = parser.parseContent(scxmlContent);
```

## 테스트 실행 방법

프로젝트는 `examples`와 `generator` 모듈 각각에 대한 테스트를 제공합니다. 테스트를 빌드하고 실행하는 방법은 다음과 같습니다:

### 모든 테스트 실행

```bash
# 테스트를 포함하여 빌드
./build.sh Debug ON

# 모든 테스트 실행
cd build
ctest
```

### 특정 테스트 그룹만 실행

```bash
cd build
ctest -R StateMachineExampleTests  # examples 테스트만 실행
ctest -R GeneratorTests            # generator 테스트만 실행
```

## 개발 목표

- **코드 생성 구현**: SCXML 파서가 생성한 모델을 기반으로 C++ 코드를 자동 생성
- **전체 SCXML 명세 지원**: 모든 SCXML 기능(병렬 상태, 이력 상태, 데이터 모델 등) 구현
- **사용자 정의 확장**: 커스텀 액션, 가드 조건 등의 사용자 정의 확장 지원

## 기여하기

1. 원하는 기능이나 개선 사항에 대한 이슈 생성
2. 프로젝트 포크 및 브랜치 생성
3. 코드 변경 및 테스트 추가
4. 풀 리퀘스트 제출

## 라이선스

이 프로젝트는 MIT 라이선스 하에 제공됩니다.
