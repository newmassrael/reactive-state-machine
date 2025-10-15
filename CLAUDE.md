## Core Development Principles

### Architecture First
- **Required Before Engine Modifications**: Always refer to ARCHITECTURE.md first before modifying Interpreter or JIT (Static) engines
- **Zero Duplication Principle**: Interpreter and JIT engines must share logic through Helper functions
- **Single Source of Truth**: Duplicate implementations prohibited, shared Helper classes required
  - Examples: SendHelper, TransitionHelper, ForeachHelper, GuardHelper

## Code Modification Rules

### StaticCodeGenerator
- Never use regex in StaticCodeGenerator, always modify directly with file editor

## Git Commit Guidelines

### Commit Message Format
- **Required Before Committing**: Always refer to COMMIT_FORMAT.md for commit message format
- Follow the project's commit message conventions and structure