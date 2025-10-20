# Commit Message Format Guide

## Structure

```
<type>: <subject>

- <detail 1>
- <detail 2>
- <detail 3>
```

## Rules

### 1. Subject Line
- Format: `<type>: <subject>`
- Types: `feat`, `refactor`, `fix`, `docs`, `test`, `chore`
- Subject: Clear and concise description of the change
- No period at the end
- Max 72 characters

### 2. Body
- One blank line after subject
- Bullet points (- prefix) only
- **1-3 items** - focus on key changes (fewer is better)
- Be specific and technical
- Reference W3C SCXML sections when applicable (e.g., "W3C SCXML 3.12.1")

### 3. Style
- **No emojis**
- **No "Generated with Claude Code"**
- **No "Co-Authored-By" tags**
- Professional and technical tone
- Focus on "what" and "why", not "how"

## Type Guidelines

| Type | When to Use | Examples |
|------|-------------|----------|
| `feat` | New features or capabilities | Add JSEngine integration, Implement foreach loops |
| `refactor` | Code restructuring without behavior change | Eliminate duplication, Extract helper functions |
| `fix` | Bug fixes | Fix type preservation, Resolve string concatenation |
| `docs` | Documentation changes | Update ARCHITECTURE.md |
| `test` | Test additions or modifications | Add W3C test 155 support |
| `chore` | Build, tooling, dependencies | Update CMake configuration |

## Examples from Project

### Good: Feature Addition
```
feat: Implement lazy initialization pattern for JSEngine integration

- Add std::optional-based lazy initialization to align with ARCHITECTURE.md
- Introduce ensureJSEngine() helper to eliminate initialization duplication
- Extract executeForeachLoop() to reduce code duplication (DRY principle)
```

### Good: Refactoring
```
refactor: Eliminate code duplication with shared helper functions

- Add common helpers (ForeachValidator, ForeachHelper, GuardHelper)
- Simplify static code generator using shared helpers
- Extract test summary function to eliminate duplication
```

### Good: Complex Feature (1-3 items)
```
feat: Run AOT engine for all 202 W3C tests with variant support

- Execute AOT engine for all tests (unsupported return FAIL)
- Preserve variant suffixes (403a, 403b, 403c) in AOT execution
- Display failed tests separately by engine type (Interpreter/AOT)
```

### Good: Concise (1-2 items when sufficient)
```
refactor: Extract TransitionHelper as Single Source of Truth

- Implement W3C SCXML 3.12 event matching in shared helper
- Eliminate 35 LOC duplication between Interpreter and AOT engines
```

### Bad: Too Many Details
```
feat: Add foreach support

- Add ForeachHelper class
- Update ActionExecutorImpl
- Update StaticCodeGenerator
- Add variable existence check
- Implement type preservation
- Add fallback handling
- Update tests
```
**Problem**: 7 items - should be condensed to 3 key changes

### Bad: Too Vague
```
refactor: Improve code

- Update files
- Fix issues
- Add features
```
**Problem**: Not specific enough - what was improved and why?

## Common Mistakes to Avoid

### ❌ Bad: Using Emojis and Attribution
```
refactor: Remove Phase markers and add code review guidelines (CLAUDE.md compliance)

- Remove all Phase 1/2/3 markers from StateMachine.cpp
- Add Code Review Guidelines section to CLAUDE.md
...

🤖 Generated with [Claude Code](https://claude.com/claude-code)

Co-Authored-By: Claude <noreply@anthropic.com>
```
**Problems**:
1. Emojis (🤖) in commit message
2. "Generated with Claude Code" footer
3. "Co-Authored-By" tag
4. Too many details (should condense to 1-3 key points)

### ✅ Good: Clean and Professional
```
refactor: Remove Phase markers and add code review guidelines

- Replace Phase 1/2/3 markers with W3C SCXML 3.13 references (CLAUDE.md compliance)
- Add Code Review Guidelines to CLAUDE.md with required reference documents
- Add test193, test242, test355 with proper CMake dependencies
```
**Why Better**:
1. No emojis or attribution tags
2. Condensed to 3 key changes
3. Each bullet is specific and technical
4. Professional tone throughout

## Recent Project Examples

### Good: Architecture Compliance
```
refactor: Implement ForeachHelper as Single Source of Truth

- Add ForeachHelper::setLoopVariable() with W3C SCXML 4.6 compliance
- Refactor Interpreter and AOT engines to use shared helper (eliminate 40+ LOC duplication)
- Fix test155 type preservation (executeScript vs setVariable for JavaScript type evaluation)
```

### Good: Feature Implementation
```
feat: Add W3C SCXML test190 with runtime expression detection

- Implement dynamic invoke detection in StaticCodeGenerator
- Generate Interpreter wrapper for tests with runtime expressions
- All 202 W3C tests pass on both Interpreter and AOT engines
```

### Good: Bug Fix
```
fix: Resolve test242sub1.scxml CMake build dependency

- Add test242sub1.scxml to GENERATED_W3C_HEADERS for proper build order
- Ensure child SCXML files exist before parent code generation
```

**Key Points**:
- 1-3 items (use fewer when sufficient)
- No emojis, no attribution tags
- Specific and technical
- References W3C SCXML section when applicable
- Quantifies impact when relevant ("40+ LOC duplication")
