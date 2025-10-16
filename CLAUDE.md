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

## Git Commit Guidelines

### Commit Message Format
- **Required Before Committing**: Always refer to COMMIT_FORMAT.md for commit message format
- Follow the project's commit message conventions and structure