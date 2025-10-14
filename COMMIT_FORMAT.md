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
- **Maximum 3 items** - focus on key changes
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

### Good: Complex Feature (still 3 items)
```
feat: Run jit engine for all 202 W3C tests with variant support

- Execute jit engine for all tests (unsupported return FAIL)
- Preserve variant suffixes (403a, 403b, 403c) in JIT execution
- Display failed tests separately by engine type (Interpreter/JIT)
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

## For This Commit

Based on our changes, the commit should be:

```
refactor: Implement ForeachHelper as Single Source of Truth

- Add ForeachHelper::setLoopVariable() with W3C SCXML 4.6 compliance
- Refactor Interpreter and JIT engines to use shared helper (eliminate 40+ LOC duplication)
- Fix test155 type preservation (executeScript vs setVariable for JavaScript type evaluation)
```

**Key Points**:
- 3 items exactly
- No emojis, no attribution tags
- Specific and technical
- References W3C SCXML section
- Quantifies impact ("40+ LOC duplication")
