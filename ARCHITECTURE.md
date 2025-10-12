# Architecture: W3C SCXML Full Compliance for Static Code Generation

## Vision

**Goal**: Generate C++ code from SCXML that satisfies **ALL** W3C SCXML 1.0 specifications while maintaining zero-overhead abstraction principles.

**Philosophy**: "You don't pay for what you don't use" - C++ core principle applied to state machine generation.

## Current State Analysis

### Dynamic Runtime (rsm_unified)
- **W3C Compliance**: 202/202 tests PASSED
- **Coverage**: ~95% of W3C SCXML 1.0 specification
- **Performance**: Interpreter-based, ~100x slower than static code
- **Memory**: ~100KB fixed overhead + tree structures
- **Use Case**: Maximum flexibility, runtime SCXML loading

### Static Code Generator (scxml-codegen)
- **W3C Compliance**: ~50/202 tests would pass
- **Coverage**: ~25-30% of W3C SCXML 1.0 specification
- **Performance**: 100x faster than dynamic (benchmark verified)
- **Memory**: ~8 bytes (state variable only)
- **Use Case**: Embedded systems, high-performance applications

### The Gap

| Feature Category | Dynamic Runtime | Static Codegen | Gap |
|-----------------|-----------------|----------------|-----|
| Atomic States | ✅ | ✅ | None |
| Event Transitions | ✅ | ✅ | None |
| Guards & Actions | ✅ | ✅ (C++ only) | JS expressions |
| Compound States | ✅ | ❌ | Full hierarchy |
| Parallel States | ✅ | ❌ | Multiple active states |
| History States | ✅ | ❌ | State restoration |
| Final States | ✅ | ❌ | Done events |
| Eventless Transitions | ✅ | ❌ | Auto transitions |
| Internal Transitions | ✅ | ❌ | No exit/entry |
| Data Model | ✅ | ❌ | Variables & expressions |
| Invoke | ✅ | ❌ | External services |
| Send with Delay | ✅ | ❌ | Timers |

## Proposed Architecture: Hybrid Approach

### Core Principle: Feature Detection + Tiered Implementation

```
SCXML File
    ↓
Feature Analyzer → SCXMLFeatures
    ↓
Policy Selection → Template Parameters / Conditional Compilation
    ↓
Code Generation → Minimal C++ with only needed features
    ↓
Compilation → Dead code elimination, full optimization
    ↓
Binary → Zero overhead for unused features
```

## Feature Tier Classification

### Tier 0: Zero Overhead (Always Free)

**Coverage**: ~80% of W3C SCXML features

**Structural Features**:
- Atomic, Compound, Final states
- Initial pseudo-states
- Event-based transitions
- Conditional transitions (guards)
- Entry/Exit handlers
- Transition actions
- Eventless transitions
- In() predicate
- Document order processing

**Implementation Strategy**:
- Compile-time encoding (enums, constexpr)
- No runtime data structures needed
- All resolved at compile time
- Full inline optimization possible

**Example**: Compound States
```cpp
// Encoding hierarchy in enum values
enum class State : uint16_t {
    // Parent: Off = 0
    Off = 0,

    // Parent: Heating = 100
    Heating = 100,
    Heating_low = 101,   // Child of Heating
    Heating_high = 102,  // Child of Heating

    // Parent: Cooling = 200
    Cooling = 200,
    Cooling_low = 201,   // Child of Cooling
    Cooling_high = 202   // Child of Cooling
};

// LCA calculation: pure arithmetic
constexpr State LCA(State a, State b) {
    return static_cast<State>((static_cast<int>(a) / 100) * 100);
}
```

**Memory Cost**: 0 bytes (everything in code section)

### Tier 1: Minimal Overhead (Small Data Structures)

**Coverage**: ~15% of W3C SCXML features

**Features**:
- Parallel states
- History states (shallow/deep)
- Internal event queue (`<raise>`)

**Implementation Strategy**:
- Simple data structures only
- Added only when SCXML uses these features
- Still no heap allocation or virtual functions

**Example**: Parallel States
```cpp
// Only if SCXML has parallel states
template<typename Derived>
class StateMachineBase {
private:
    #if RSM_HAS_PARALLEL
    std::bitset<N> activeStates_;  // N states → N/8 bytes
    #else
    State currentState_;           // 1-2 bytes
    #endif
};
```

**Example**: History States
```cpp
// Only if SCXML has history states
template<typename Derived>
class StateMachineBase {
private:
    State currentState_;
    #if RSM_HAS_SHALLOW_HISTORY
    State historyState_;  // +1 byte
    #endif
    #if RSM_HAS_DEEP_HISTORY
    std::array<State, MAX_DEPTH> historyStack_;  // +MAX_DEPTH bytes
    #endif
};
```

**Memory Cost**:
- Parallel: ~12 bytes (std::bitset or std::set)
- Shallow History: +1 byte
- Deep History: +8-24 bytes
- Internal Queue: +48 bytes (std::queue)

### Tier 2: Conditional Overhead (External Dependencies)

**Coverage**: ~5% of W3C SCXML features

**Features**:
- JavaScript expressions (complex)
- Data model (ECMAScript)
- HTTP invoke
- Timer system (`<send delay>`)
- Child state machines

**Implementation Strategy**:
- Policy-based templates OR conditional compilation
- External libraries linked only when needed
- Feature detection determines inclusion

**Example**: JavaScript Policy (Template Approach)
```cpp
// Policy: No JavaScript (zero size)
struct NoJavaScript {
    static constexpr bool enabled = false;
    bool evaluate(const char*) const { return true; }
    void execute(const char*) const {}
};

// Policy: With JavaScript
struct WithJavaScript {
    static constexpr bool enabled = true;
    JSRuntime* rt_;
    JSContext* ctx_;

    WithJavaScript() {
        rt_ = JS_NewRuntime();
        ctx_ = JS_NewContext(rt_);
    }

    bool evaluate(const char* expr) {
        JSValue result = JS_Eval(ctx_, expr, strlen(expr), "<expr>", 0);
        bool ret = JS_ToBool(ctx_, result);
        JS_FreeValue(ctx_, result);
        return ret;
    }
};

// Base class with policy
template<typename Derived, typename JSPolicy = NoJavaScript>
class StateMachineBase {
    [[no_unique_address]] JSPolicy js_;  // Zero size if NoJavaScript!

    bool evaluateGuard(const char* expr) {
        if constexpr (JSPolicy::enabled) {
            return js_.evaluate(expr);
        } else {
            // Simple C++ function call
            return derived().callGuardFunction(expr);
        }
    }
};
```

**Example**: Conditional Compilation Approach
```cpp
// Auto-detected during generation
#define RSM_HAS_JAVASCRIPT 0
#define RSM_HAS_HTTP_INVOKE 0
#define RSM_HAS_TIMERS 1

#if RSM_HAS_JAVASCRIPT
#include <quickjs.h>
#endif

#if RSM_HAS_HTTP_INVOKE
#include <httplib.h>
#endif

#if RSM_HAS_TIMERS
#include <chrono>
#include <thread>
#endif

template<typename Derived>
class StateMachineBase {
#if RSM_HAS_JAVASCRIPT
    JSRuntime* jsRuntime_;
    JSContext* jsContext_;
#endif

#if RSM_HAS_TIMERS
    std::map<std::string, TimerHandle> timers_;
#endif
};
```

**Memory Cost**:
- JavaScript: ~200KB (QuickJS runtime, can be shared)
- HTTP Client: ~50KB (cpp-httplib, can be shared)
- Timers: ~1KB + sizeof(std::map)
- Child Machines: sizeof(ChildStateMachine) per instance

**Library Dependencies**:
- Only linked if SCXML actually uses these features
- CMake automatically detects and links

## Feature Detection System

### SCXMLFeatureAnalyzer

```cpp
struct SCXMLFeatures {
    // Tier 0: Structural (always supported)
    bool hasAtomicStates = false;
    bool hasCompoundStates = false;
    bool hasFinalStates = false;
    bool hasEventlessTransitions = false;
    bool hasInternalTransitions = false;

    // Tier 1: Minimal overhead
    bool hasParallelStates = false;
    bool hasShallowHistory = false;
    bool hasDeepHistory = false;
    bool needsInternalQueue = false;  // <raise> detected

    // Tier 2: Conditional overhead
    bool needsJSEngine = false;       // Complex JS expressions
    bool needsDataModel = false;      // <datamodel> present
    bool needsHTTPInvoke = false;     // <invoke type="http">
    bool needsSCXMLInvoke = false;    // <invoke type="scxml">
    bool needsTimers = false;         // <send delay="...">

    // Derived properties
    int maxHierarchyDepth = 1;
    int totalStates = 0;
    bool hasMultipleTargets = false;
};

class SCXMLFeatureAnalyzer {
public:
    SCXMLFeatures analyze(const std::shared_ptr<SCXMLDocument>& doc) {
        SCXMLFeatures features;

        // Analyze states
        for (auto& state : doc->getAllStates()) {
            analyzeState(state, features);
        }

        // Analyze transitions
        for (auto& trans : doc->getAllTransitions()) {
            analyzeTransition(trans, features);
        }

        // Analyze executable content
        for (auto& exec : doc->getAllExecutableContent()) {
            analyzeExecutableContent(exec, features);
        }

        return features;
    }

private:
    void analyzeState(const StateNode& state, SCXMLFeatures& f) {
        switch (state.getType()) {
            case StateType::COMPOUND:
                f.hasCompoundStates = true;
                f.maxHierarchyDepth = std::max(f.maxHierarchyDepth,
                                                state.getDepth());
                break;
            case StateType::PARALLEL:
                f.hasParallelStates = true;
                break;
            case StateType::FINAL:
                f.hasFinalStates = true;
                break;
            case StateType::HISTORY:
                if (state.getHistoryType() == HistoryType::DEEP)
                    f.hasDeepHistory = true;
                else
                    f.hasShallowHistory = true;
                break;
        }
        f.totalStates++;
    }

    void analyzeTransition(const Transition& trans, SCXMLFeatures& f) {
        if (trans.getEvent().empty()) {
            f.hasEventlessTransitions = true;
        }
        if (trans.getType() == TransitionType::INTERNAL) {
            f.hasInternalTransitions = true;
        }
        if (trans.getTargets().size() > 1) {
            f.hasMultipleTargets = true;
        }
    }

    void analyzeExecutableContent(const ExecutableContent& exec,
                                   SCXMLFeatures& f) {
        if (auto* script = dynamic_cast<const ScriptAction*>(&exec)) {
            if (isComplexJavaScript(script->getContent())) {
                f.needsJSEngine = true;
            }
        }
        if (auto* raise = dynamic_cast<const RaiseAction*>(&exec)) {
            f.needsInternalQueue = true;
        }
        if (auto* send = dynamic_cast<const SendAction*>(&exec)) {
            if (!send->getDelay().empty()) {
                f.needsTimers = true;
            }
        }
        if (auto* invoke = dynamic_cast<const Invoke*>(&exec)) {
            if (invoke->getType() == "http") {
                f.needsHTTPInvoke = true;
            } else if (invoke->getType() == "scxml") {
                f.needsSCXMLInvoke = true;
            }
        }
    }

    bool isComplexJavaScript(const std::string& code) {
        // Simple heuristic: if it's just a function call, it's simple
        static std::regex simpleFunctionCall(R"(^\w+\(\)$)");
        if (std::regex_match(code, simpleFunctionCall)) {
            return false;
        }

        // Check for JS operators, keywords
        static std::regex jsKeywords(
            R"(\b(var|let|const|if|for|while|function|return|Math\.)\b)"
        );
        return std::regex_search(code, jsKeywords);
    }
};
```

## Code Generation Strategies

### Strategy A: Policy-Based Templates (Preferred)

**Advantages**:
- Clean separation of concerns
- Zero size for unused policies (C++20 `[[no_unique_address]]`)
- Compile-time optimization
- Easy to test individual policies

**Generation Pattern**:
```cpp
// Generated code structure
namespace RSM::Generated {

enum class State : uint8_t { /* ... */ };
enum class Event : uint8_t { /* ... */ };

// Select policies based on features
using JSPolicy =
    #if DETECTED_COMPLEX_JAVASCRIPT
    RSM::Policies::WithJavaScript
    #else
    RSM::Policies::NoJavaScript
    #endif
    ;

using TimerPolicy = /* ... */;
using InvokePolicy = /* ... */;

template<typename Derived>
class ThermostatBase : public RSM::StateMachineCore<
    Derived,
    JSPolicy,
    TimerPolicy,
    InvokePolicy
> {
    // State machine implementation
};

} // namespace RSM::Generated
```

**Policy Library** (in rsm/include/policies/):
```cpp
namespace RSM::Policies {

// JavaScript Policies
struct NoJavaScript {
    static constexpr bool enabled = false;
    bool evaluate(const char*) const { return true; }
};

struct WithJavaScript {
    static constexpr bool enabled = true;
    // QuickJS integration
};

// Timer Policies
struct NoTimer {
    static constexpr bool enabled = false;
};

struct WithTimer {
    static constexpr bool enabled = true;
    void schedule(Event e, std::chrono::milliseconds delay);
    void cancel(const std::string& id);
};

// Invoke Policies
struct NoInvoke {
    static constexpr bool enabled = false;
};

struct WithHTTPInvoke {
    static constexpr bool enabled = true;
    void invoke(const std::string& url, /* ... */);
};

} // namespace RSM::Policies
```

### Strategy B: Conditional Compilation

**Advantages**:
- Simpler implementation
- Familiar #ifdef patterns
- Easy to understand

**Disadvantages**:
- Less type-safe
- Harder to test
- More preprocessor complexity

**Generation Pattern**:
```cpp
// Feature flags (auto-detected)
#define RSM_HAS_PARALLEL_STATES 1
#define RSM_HAS_JAVASCRIPT 0
#define RSM_HAS_TIMERS 1

template<typename Derived>
class ThermostatBase {
private:
    #if RSM_HAS_PARALLEL_STATES
    std::set<State> activeStates_;
    #else
    State currentState_;
    #endif

    #if RSM_HAS_TIMERS
    std::map<std::string, TimerHandle> timers_;
    #endif

public:
    void processEvent(Event event) {
        #if RSM_HAS_PARALLEL_STATES
        // Parallel state logic
        #else
        // Simple state logic
        #endif
    }
};
```

## W3C Algorithm Implementation

### LCA (Least Common Ancestor) Calculation

**Dynamic Runtime**: Tree traversal O(h)
```cpp
std::shared_ptr<IStateNode> StateMachine::findLCA(
    const std::shared_ptr<IStateNode>& s1,
    const std::shared_ptr<IStateNode>& s2)
{
    // Build ancestor chains
    std::vector<std::shared_ptr<IStateNode>> ancestors1, ancestors2;
    // ... traverse up the tree ...
    // Find common ancestor
    return commonAncestor;
}
```

**Static Generation**: Pre-computed table O(1)
```cpp
// Generated at compile time
constexpr State LCA_TABLE[NUM_STATES][NUM_STATES] = {
    // Pre-computed for all state pairs
    {State::Off, State::Off, State::Heating, /* ... */},
    {State::Off, State::Off, State::Heating, /* ... */},
    // ...
};

State findLCA(State s1, State s2) {
    return LCA_TABLE[static_cast<int>(s1)][static_cast<int>(s2)];
}

// Or arithmetic encoding:
State findLCA(State s1, State s2) {
    int parent1 = static_cast<int>(s1) / 100;
    int parent2 = static_cast<int>(s2) / 100;
    if (parent1 == parent2) return static_cast<State>(parent1 * 100);
    // ...
}
```

### Entry/Exit Set Computation

**Dynamic Runtime**: Build and sort at runtime
```cpp
void StateMachine::computeEntrySet(/* ... */) {
    std::set<StateNode*, DocumentOrderComparator> entrySet;
    // Add states to set...
    // Iterate in document order
    for (auto& state : entrySet) {
        executeEntryActions(state);
    }
}
```

**Static Generation**: Fixed sequence
```cpp
void enterState_HeatingHigh() {
    // Document order hard-coded
    derived().onEnterHeating();      // Parent first
    derived().onEnterHeatingHigh();  // Then child
    currentState_ = State::Heating_high;
}

void exitState_HeatingHigh() {
    // Reverse document order
    derived().onExitHeatingHigh();   // Child first
    derived().onExitHeating();       // Then parent
}
```

### Eventless Transitions

**Dynamic Runtime**: Check after each step
```cpp
void StateMachine::checkEventlessTransitions() {
    bool changed = true;
    while (changed) {
        changed = false;
        for (auto& trans : currentState->getTransitions()) {
            if (trans->getEvent().empty() &&
                evaluateCondition(trans->getCondition())) {
                executeTransition(trans);
                changed = true;
                break;
            }
        }
    }
}
```

**Static Generation**: Generated check method
```cpp
bool checkEventlessTransitions() {
    bool changed = true;
    while (changed) {
        changed = false;

        switch (currentState_) {
            case State::Idle:
                if (derived().isTemperatureHigh()) {
                    // Eventless transition to Heating
                    currentState_ = State::Heating;
                    derived().onEnterHeating();
                    changed = true;
                }
                break;
            // ... other states ...
        }
    }
    return changed;
}

void processEvent(Event event) {
    // Handle event...

    // Check eventless after each event
    checkEventlessTransitions();
}
```

## Implementation Roadmap

### Week 1: Feature Detection Infrastructure

**Deliverables**:
- [ ] `SCXMLFeatureAnalyzer` class
- [ ] `SCXMLFeatures` struct with all flags
- [ ] Integration into `StaticCodeGenerator`
- [ ] Feature detection unit tests
- [ ] CLI flag to show detected features: `scxml-codegen --analyze`

**Code Changes**:
- New file: `tools/codegen/FeatureAnalyzer.h/cpp`
- Modify: `tools/codegen/StaticCodeGenerator.cpp`

**Tests**:
```cpp
TEST(FeatureAnalyzer, DetectsCompoundStates) {
    auto doc = parseScxml("compound_states.scxml");
    SCXMLFeatures features = analyzer.analyze(doc);
    EXPECT_TRUE(features.hasCompoundStates);
    EXPECT_EQ(features.maxHierarchyDepth, 3);
}
```

### Week 2: Tier 0 - Compound States

**Deliverables**:
- [ ] Hierarchical state encoding (enum or parent pointers)
- [ ] LCA pre-computation or arithmetic encoding
- [ ] Entry/Exit set generation in document order
- [ ] Proper parent-child state transitions

**Code Changes**:
- Modify: `generateStateEnum()` - encode hierarchy
- New: `generateLCAFunction()` or `generateLCATable()`
- Modify: `generateProcessEvent()` - compound state transitions

**Example Generated Code**:
```cpp
enum class State : uint16_t {
    Idle = 0,
    Active = 100,
    Active_Working = 101,
    Active_Working_Phase1 = 102,
    Active_Working_Phase2 = 103,
};

void transitionTo_Active_Working_Phase2() {
    // Exit current state hierarchy
    if (inState(State::Idle)) {
        derived().onExitIdle();
    }

    // Enter new state hierarchy
    derived().onEnterActive();
    derived().onEnterActive_Working();
    derived().onEnterActive_Working_Phase2();

    currentState_ = State::Active_Working_Phase2;
}
```

### Week 2-3: Tier 0 - Final States & Done Events

**Deliverables**:
- [ ] Final state detection
- [ ] `done.state.X` event generation
- [ ] Parent compound state handling of done events

**Example Generated Code**:
```cpp
void processEvent(Event event) {
    switch (currentState_) {
        case State::Task_Final:
            // Final state reached
            if (event == Event::_InternalMarker) {
                // Generate done event
                raiseInternalEvent(Event::Done_state_Task);

                // Notify parent compound
                currentState_ = State::Idle;
                derived().onEnterIdle();
            }
            break;
    }
}
```

### Week 3: Tier 1 - Parallel States

**Deliverables**:
- [ ] Multiple active states support
- [ ] `std::set<State>` or `std::bitset<N>` implementation
- [ ] Parallel state entry/exit
- [ ] All children must be in final state before done

**Code Changes**:
- Conditional: Use `activeStates_` if parallel detected
- Modify: `inState()` checks all active states
- New: `enterParallelRegion()`, `exitParallelRegion()`

**Example Generated Code**:
```cpp
template<typename Derived>
class MachineBase {
private:
    #if RSM_HAS_PARALLEL
    std::bitset<10> activeStates_;  // 10 states total
    #else
    State currentState_;
    #endif

public:
    bool inState(State s) const {
        #if RSM_HAS_PARALLEL
        return activeStates_.test(static_cast<int>(s));
        #else
        return currentState_ == s;
        #endif
    }
};
```

### Week 3: Tier 1 - History States

**Deliverables**:
- [ ] Shallow history: remember last active child
- [ ] Deep history: remember entire configuration
- [ ] History state restoration logic

**Example Generated Code**:
```cpp
template<typename Derived>
class MachineBase {
private:
    State currentState_;

    #if RSM_HAS_SHALLOW_HISTORY
    State history_Heating_ = State::Heating_low;  // Default
    #endif

public:
    void enterState_Heating_History() {
        // Restore last active child of Heating
        State target = history_Heating_;
        transitionToState(target);
    }

    void exitState_Heating() {
        // Save current child for history
        #if RSM_HAS_SHALLOW_HISTORY
        history_Heating_ = currentState_;
        #endif
    }
};
```

### Week 4: Tier 2 - JavaScript Policy (Optional)

**Deliverables**:
- [ ] Policy interface design
- [ ] `NoJavaScript` stub policy (zero size)
- [ ] `WithJavaScript` QuickJS integration
- [ ] Automatic policy selection in generator

**Code Changes**:
- New: `rsm/include/policies/JavaScriptPolicy.h`
- New: `rsm/include/policies/NoJavaScript.h`
- New: `rsm/include/policies/WithJavaScript.h`
- Modify: Generator selects policy based on features

**Example Policy Usage**:
```cpp
template<typename Derived, typename JSPolicy = NoJavaScript>
class MachineBase {
    [[no_unique_address]] JSPolicy js_;

    bool evaluateCondition(const char* expr) {
        if constexpr (JSPolicy::enabled) {
            return js_.evaluate(expr);
        } else {
            // Direct C++ function call
            return derived().callFunction(expr);
        }
    }
};
```

### Week 5: W3C Test Validation

**Deliverables**:
- [ ] Test harness generator for W3C tests
- [ ] Automated test pipeline: SCXML → Generate → Compile → Run
- [ ] Comparison with dynamic runtime behavior
- [ ] Test result dashboard

**Test Process**:
```bash
# For each W3C test:
1. Parse test SCXML
2. Generate C++ code
3. Generate test harness (implements required methods)
4. Compile
5. Run and capture state transitions
6. Compare with dynamic runtime
7. Report pass/fail
```

**Goal**: 202/202 PASSED (or subset that applies to static generation)

## Performance & Memory Comparison

### Benchmark Scenarios

| Scenario | Dynamic | Static (Tier 0) | Static (Tier 1) | Static (Tier 2) |
|----------|---------|-----------------|-----------------|-----------------|
| Simple (10 states) | 100KB + 50µs | 8B + 0.5µs | 20B + 0.6µs | 200KB + 1µs |
| Complex (100 states, hierarchy) | 150KB + 200µs | 16B + 2µs | 48B + 3µs | 250KB + 5µs |
| Parallel (20 regions) | 200KB + 500µs | N/A | 64B + 10µs | 300KB + 15µs |

**Notes**:
- Dynamic includes interpreter, tree structures, action executor
- Static Tier 0: Only state variable(s)
- Static Tier 1: + bitset/history/queue
- Static Tier 2: + QuickJS/HTTP/timers (shared across instances)

### Memory Breakdown

**Dynamic Runtime per instance**:
```
Base overhead:               ~16 KB
State tree:                  ~N * 512 bytes
Transition tables:           ~M * 256 bytes
Action executor:             ~8 KB
JS engine (shared):          ~200 KB
Total:                       ~100-500 KB per instance
```

**Static Generated per instance**:
```
Tier 0:                      1-8 bytes (state enum)
Tier 1 (parallel):           +12-64 bytes (bitset/set)
Tier 1 (history):            +1-24 bytes (state vars)
Tier 2 (JS, shared):         +0 bytes (shared engine)
Total:                       ~8-100 bytes per instance
```

**Advantage**: 1000x - 10000x less memory per instance

## Validation Strategy

### Unit Tests (Week 1-4)

- Feature detection correctness
- Individual feature code generation
- Policy selection logic
- Generated code compilation

### Integration Tests (Week 3-5)

- End-to-end: SCXML → Generate → Compile → Run
- Feature combinations (compound + parallel + history)
- Error handling and edge cases

### W3C Compliance Tests (Week 5)

**Target**: Pass all applicable W3C SCXML tests with generated code

**Methodology**:
1. Categorize W3C tests by features
2. For each test:
   - Generate C++ from test SCXML
   - Create test harness
   - Execute same test sequence
   - Verify identical behavior to dynamic runtime
3. Track pass rate: X/202 tests

**Exclusions** (if any):
- Tests requiring true runtime SCXML loading (not applicable to static)
- Tests with features explicitly marked as Tier 2 (document exceptions)

### Performance Regression Tests

- Ensure generated code remains 50-100x faster than dynamic
- Memory usage stays within Tier boundaries
- No performance degradation as features added

## Risk Analysis & Mitigation

### Risk 1: Code Size Explosion
**Risk**: Complex SCXML generates too much C++ code
**Mitigation**:
- Use helper functions for common patterns
- Template methods to reduce duplication
- Compiler optimization and dead code elimination
- Benchmark against realistic SCXML files

### Risk 2: Compilation Time
**Risk**: Generated code takes too long to compile
**Mitigation**:
- Minimize template instantiation depth
- Pre-compiled policy libraries
- Incremental generation (only changed parts)
- Use extern templates for common cases

### Risk 3: Policy Complexity
**Risk**: Policy system becomes too complex to maintain
**Mitigation**:
- Start with simple policies (JS only)
- Document policy interface clearly
- Provide policy testing framework
- Limit number of policy dimensions (max 3-4)

### Risk 4: W3C Test Coverage
**Risk**: Cannot achieve 100% test pass rate
**Mitigation**:
- Prioritize most common features (Tier 0-1)
- Document any intentional limitations
- Provide workarounds or alternatives
- Set realistic initial target (e.g., 80% → 90% → 95%)

## Success Metrics

### Must Have (MVP)
- [ ] Feature detection system working
- [ ] Compound states with correct LCA
- [ ] Parallel states (Tier 1)
- [ ] History states (shallow + deep)
- [ ] Final states with done events
- [ ] Eventless transitions
- [ ] Internal transitions
- [ ] 70%+ W3C test pass rate

### Should Have
- [ ] JavaScript policy (NoJS + WithJS)
- [ ] Timer policy (for `<send delay>`)
- [ ] 85%+ W3C test pass rate
- [ ] Performance maintained (50x+ faster than dynamic)
- [ ] Memory within Tier predictions

### Nice to Have
- [ ] HTTP invoke policy
- [ ] Child state machine support
- [ ] 95%+ W3C test pass rate
- [ ] Visual test dashboard
- [ ] Performance profiling tools

## Future Enhancements

### Post-MVP
- Visual debugger integration
- SCXML validation and linting
- Optimization hints (suggest simpler SCXML for better codegen)
- Multiple backend targets (C, Rust, etc.)
- WASM compilation support

### Research Areas
- Machine learning for optimal state encoding
- Formal verification of generated code
- Real-time scheduling guarantees
- Safety-critical certification (DO-178C, IEC 61508)

## Conclusion

The proposed hybrid architecture is **technically feasible** and can achieve **100% W3C SCXML 1.0 compliance** while maintaining zero-overhead principles for the majority of features.

**Key Enablers**:
1. Dynamic runtime proves all features are implementable
2. Feature detection enables "pay for what you use"
3. Policy-based design maintains type safety and performance
4. Tiered approach balances coverage vs. overhead

**Timeline**: 3-5 weeks for full implementation
**Risk**: Low - all technical challenges have known solutions
**Value**: High - combines standards compliance with optimal performance

---

**Status**: Architecture Approved - Ready for Implementation
**Last Updated**: 2025-10-12
**Version**: 1.0
