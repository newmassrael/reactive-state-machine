#!/bin/bash

# Setup git hooks for the reactive-state-machine project
# This script installs pre-commit hooks for automatic code formatting

set -e

PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
HOOKS_DIR="$PROJECT_ROOT/.git/hooks"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[0;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

echo -e "${BLUE}Setting up git hooks for reactive-state-machine...${NC}"

# Check if we're in a git repository
if [ ! -d "$PROJECT_ROOT/.git" ]; then
    echo -e "${RED}Error: Not in a git repository${NC}"
    exit 1
fi

# Check if clang-format is available
if ! command -v clang-format &> /dev/null; then
    echo -e "${YELLOW}Warning: clang-format not found.${NC}"
    echo "Please install clang-format for automatic code formatting:"
    echo "  Ubuntu/Debian: sudo apt install clang-format"
    echo "  Fedora/RHEL:   sudo dnf install clang-tools-extra"
    echo "  macOS:         brew install clang-format"
    echo ""
    echo -e "${YELLOW}Continuing without clang-format check...${NC}"
fi

# Create pre-commit hook
cat > "$HOOKS_DIR/pre-commit" << 'EOF'
#!/bin/bash

# Pre-commit hook to automatically format code with clang-format
# This will format all staged C++ files before commit

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[0;33m'
NC='\033[0m' # No Color

# Check if clang-format is available
if ! command -v clang-format &> /dev/null; then
    echo -e "${RED}Error: clang-format not found. Please install clang-format.${NC}"
    echo "Ubuntu/Debian: sudo apt install clang-format"
    echo "Fedora/RHEL: sudo dnf install clang-tools-extra"
    echo "macOS: brew install clang-format"
    exit 1
fi

# Get the list of staged files
staged_files=$(git diff --cached --name-only --diff-filter=ACM | grep -E '\.(cpp|cc|cxx|c|hpp|h|hxx)$')

if [ -z "$staged_files" ]; then
    # No C++ files to format
    exit 0
fi

echo -e "${YELLOW}Running clang-format on staged C++ files...${NC}"

# Flag to track if any files were modified
files_modified=false

# Process each staged file
for file in $staged_files; do
    if [ -f "$file" ]; then
        # Check if file needs formatting
        clang-format "$file" | diff -u "$file" - > /dev/null
        if [ $? -ne 0 ]; then
            echo -e "${YELLOW}Formatting: $file${NC}"
            # Format the file
            clang-format -i "$file"
            # Re-stage the formatted file
            git add "$file"
            files_modified=true
        fi
    fi
done

if [ "$files_modified" = true ]; then
    echo -e "${GREEN}Code formatting completed. Files have been re-staged.${NC}"
    echo -e "${YELLOW}Note: Your commit includes the formatted changes.${NC}"
else
    echo -e "${GREEN}All staged C++ files are already properly formatted.${NC}"
fi

# Continue with the commit
exit 0
EOF

# Make the hook executable
chmod +x "$HOOKS_DIR/pre-commit"

echo -e "${GREEN}✓ Pre-commit hook installed successfully${NC}"

# Optional: Create commit-msg hook for commit message validation
if [ "$1" == "--with-commit-msg" ]; then
    cat > "$HOOKS_DIR/commit-msg" << 'EOF'
#!/bin/bash

# Commit message hook to ensure proper commit message format
# Following conventional commits style

commit_regex='^(feat|fix|docs|style|refactor|test|chore|ci|build|perf)(\(.+\))?: .{1,50}'

if ! grep -qE "$commit_regex" "$1"; then
    echo "Invalid commit message format!"
    echo ""
    echo "Commit message should follow the pattern:"
    echo "type(scope): description"
    echo ""
    echo "Types: feat, fix, docs, style, refactor, test, chore, ci, build, perf"
    echo "Example: feat(parser): add support for parallel states"
    echo "Example: fix(runtime): resolve memory leak in event processing"
    echo ""
    exit 1
fi
EOF
    chmod +x "$HOOKS_DIR/commit-msg"
    echo -e "${GREEN}✓ Commit message hook installed successfully${NC}"
fi

echo ""
echo -e "${BLUE}Git hooks setup completed!${NC}"
echo ""
echo "What happens now:"
echo "• Before each commit, clang-format will automatically format staged C++ files"
echo "• If files are modified, they will be re-staged automatically"
echo "• The commit will proceed with properly formatted code"
echo ""
echo "To skip formatting for a specific commit (not recommended):"
echo "git commit --no-verify -m \"your message\""
echo ""
echo "To manually format all files:"
echo "find . -name '*.cpp' -o -name '*.h' -o -name '*.hpp' | xargs clang-format -i"