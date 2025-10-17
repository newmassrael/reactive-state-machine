## Core Development Principles

### Architecture First
- **Required Before Engine Modifications**: Always refer to ARCHITECTURE.md first before modifying Interpreter or JIT (Static) engines
- **Zero Duplication Principle**: Interpreter and JIT engines must share logic through Helper functions
- **Single Source of Truth**: Duplicate implementations prohibited, shared Helper classes required
  - Examples: SendHelper, TransitionHelper, ForeachHelper, GuardHelper

## Code Modification Rules

### StaticCodeGenerator
- Never use regex in StaticCodeGenerator, always modify directly with file editor

### Code Comments and Documentation
- **No Phase Markers**: Never use "Phase 1", "Phase 2", "Phase 3", "Phase 4" in production code comments or documentation
- **No Temporary Comments**: Avoid temporary markers like "TODO Phase X", "Coming in Phase Y"
- **Production-Ready Only**: All comments must be permanent, production-appropriate documentation
- **W3C References**: Use W3C SCXML specification references instead (e.g., "W3C SCXML 3.12.1", "W3C SCXML C.1")
- **Architecture References**: Reference ARCHITECTURE.md sections for context, not development phases
- **Examples**:
  - ❌ Bad: `// Phase 4: Event scheduler polling`
  - ✅ Good: `// W3C SCXML 6.2: Event scheduler for delayed send`
  - ❌ Bad: `// TODO: Implement in Phase 5`
  - ✅ Good: `// W3C SCXML 3.4: Parallel state support (see ARCHITECTURE.md Future Components)`

## Test Integration Guidelines

### Verifying Static Code Generation Capability
**Before adding tests to CMakeLists.txt**, verify whether they can use static generation or require Interpreter wrappers.

**Correct Verification Method**:
```bash
# 1. Convert TXML to SCXML
mkdir -p /tmp/test_verify
build/tools/txml_converter/txml-converter resources/XXX/testXXX.txml /tmp/test_verify/testXXX.scxml

# 2. Try static code generation
env SPDLOG_LEVEL=warn build/tools/codegen/scxml-codegen /tmp/test_verify/testXXX.scxml -o /tmp/test_verify/

# 3. Check for warnings
# - If "has no initial state - generating Interpreter wrapper" → needs wrapper
# - If "not found in model" → needs wrapper
# - If no warnings → static generation OK
```

**What to Look For in SCXML**:
- ✅ **Static generation OK**: All event names, delays, targets are static strings
- ❌ **Needs wrapper**: Dynamic expressions (srcexpr, delayexpr, contentexpr), no initial state, invalid initial state

**Common Mistake**:
- ❌ Wrong: Testing `build/tests/w3c_static_generated/testXXX.scxml` (doesn't exist until registered)
- ✅ Correct: Convert from `resources/XXX/testXXX.txml` first

### Adding W3C Tests with Pure Static Generation
**When**: Static code generation succeeds (all features are static, no dynamic expressions)

**Required Steps**:
1. **Add to `tests/CMakeLists.txt`**:
   - Use `rsm_generate_static_w3c_test(TEST_NUM ${STATIC_W3C_OUTPUT_DIR})`
   - Add W3C SCXML specification reference comment
   - Example:
     ```cmake
     rsm_generate_static_w3c_test(279 ${STATIC_W3C_OUTPUT_DIR})  # W3C SCXML 5.2.2: early binding variable initialization
     ```

2. **Create JIT Test Registry File** `tests/w3c/jit_tests/TestXXX.h`:
   - Follow SimpleJitTest pattern for standard tests
   - Include W3C SCXML specification reference in docstring
   - Example:
     ```cpp
     #pragma once
     #include "SimpleJitTest.h"
     #include "testXXX_sm.h"

     namespace RSM::W3C::JitTests {

     /**
      * @brief W3C SCXML X.Y.Z: Feature description
      *
      * Detailed test description referencing W3C SCXML spec.
      */
     struct TestXXX : public SimpleJitTest<TestXXX, XXX> {
         static constexpr const char *DESCRIPTION = "Feature name (W3C X.Y.Z JIT)";
         using SM = RSM::Generated::testXXX::testXXX;
     };

     // Auto-register
     inline static JitTestRegistrar<TestXXX> registrar_TestXXX;

     }  // namespace RSM::W3C::JitTests
     ```

3. **Add to `tests/w3c/jit_tests/AllJitTests.h`**:
   - Include new test header in appropriate section
   - Example:
     ```cpp
     #include "Test278.h"
     #include "Test279.h"  // Add here
     ```

4. **Result**:
   - Static code generated to `build/tests/w3c_static_generated/testXXX_sm.h`
   - JIT test auto-registered via `JitTestRegistrar`
   - Both Interpreter and JIT tests pass with pure static code

### Adding W3C Tests Requiring Interpreter Wrappers
**When**: Static code generation fails (no initial state, dynamic invoke, parallel initial state format, etc.)

**Required Steps**:
1. **Add to `tests/CMakeLists.txt`**:
   - Use `rsm_generate_static_w3c_test(TEST_NUM ${STATIC_W3C_OUTPUT_DIR})`
   - Add comment explaining why wrapper is needed
   - Examples:
     ```cmake
     rsm_generate_static_w3c_test(355 ${STATIC_W3C_OUTPUT_DIR})  # no initial state
     rsm_generate_static_w3c_test(364 ${STATIC_W3C_OUTPUT_DIR})  # parallel initial state format
     ```

2. **Add to `tests/w3c/W3CTestRunner.cpp`**:
   - Add test case to `runJitTest()` dynamic invoke section
   - Ensures JIT engine recognizes these as Interpreter wrapper tests
   - Example:
     ```cpp
     case 355:
     case 364:
         LOG_WARN("W3C JIT Test: Test {} uses dynamic invoke - tested via Interpreter engine", testId);
         report.validationResult = ValidationResult(true, TestResult::PASS, "Tested via Interpreter engine (dynamic invoke)");
         report.executionContext.finalState = "pass";
         return report;
     ```

3. **Result**:
   - Wrapper generated automatically by StaticCodeGenerator
   - Test runs using perfect Interpreter engine
   - Both Interpreter and JIT tests pass

**Common Scenarios**:
- No initial state: W3C SCXML 3.6 defaults to first child in document order
- Dynamic invoke: `<invoke srcexpr>`, `<invoke><content>`, `<invoke contentExpr>`
- Parallel initial state: Space-separated state IDs (e.g., "s11p112 s11p122")
- Invalid initial state: Initial state not found in model

## Code Review Guidelines

### Required References for Code Review
**When performing code reviews**, always refer to these documents in order:

1. **ARCHITECTURE.md** - Core architecture principles and design decisions
   - Zero Duplication Principle (Helper functions)
   - All-or-Nothing Strategy (JIT vs Interpreter)
   - Feature Handling Strategy (Static vs Dynamic)
   - Single Source of Truth requirements

2. **CLAUDE.md** - Code quality and documentation standards
   - No Phase Markers rule
   - W3C SCXML references required
   - StaticCodeGenerator modification rules
   - Test Integration Guidelines

3. **COMMIT_FORMAT.md** - Git commit message conventions
   - Semantic commit prefixes
   - Descriptive messages
   - Professional language

### Code Review Checklist
- [ ] **Architecture Adherence**: Zero Duplication achieved via Helper functions?
- [ ] **Phase Markers**: No "Phase 1/2/3/4" in code or comments?
- [ ] **W3C References**: All comments use W3C SCXML specification references?
- [ ] **StaticCodeGenerator**: Direct file editing used (no regex)?
- [ ] **Test Integration**:
  - [ ] Static tests: CMakeLists.txt + JIT registry (TestXXX.h + AllJitTests.h)?
  - [ ] Wrapper tests: CMakeLists.txt + W3CTestRunner.cpp dynamic invoke section?
- [ ] **Implementation Completeness**: No TODO, no partial features, no placeholders?
- [ ] **Git Quality**: Semantic commits with professional descriptions?

### Review Output Location
- Place all code review reports in `claudedocs/` directory
- Format: `code_review_YYYY_MM_DD.md`
- Include compliance scores and action items

## Git Commit Guidelines

### Commit Message Format
- **Required Before Committing**: Always refer to COMMIT_FORMAT.md for commit message format
- Follow the project's commit message conventions and structure