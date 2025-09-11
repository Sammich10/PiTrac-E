# This script will run uncrustify on all source files in the src/ directory and check for formatting issues.
# Default behavior is to check files changed in the current branch against the master branch, but can be overridden to check all files in the src/ directory.

SOURCE_FILES=
if [ -z "$UNCRUST_CONFIG" ]; then
  UNCRUST_CONFIG=".github/workflows/uncrustify.cfg"
fi
AUTO_FIX=false
while [[ $# -gt 0 ]]; do
    case $1 in
        -a|--all)
            echo "Checking all source files in src/ directory."
            SOURCE_FILES=$(find src/ -name "*.cpp" -o -name "*.h" -o -name "*.hpp" -o -name "*.c")
            shift
            ;;
        -c|--changed)
            echo "Checking only changed source files against master branch."
            SOURCE_FILES=$(git diff --name-only origin/master 2> /dev/null | grep -E '\.(cpp|h|hpp|c)$')
            shift
            ;;
        -y|--yes)
            echo "Automatically fixing formatting issues without prompting."
            AUTO_FIX=true
            shift
            ;;
        -h|--help)
            echo "Usage: $0 [-a|--all] | [-c|--changed] [-y|--yes]"
            echo "Options:"
            echo "  -a, --all       Check all source files in the src/ directory"
            echo "  -c, --changed   Check only source files changed against the master branch"
            echo "  -y, --yes       Automatically fix formatting issues without prompting"
            echo "  -h, --help      Show this help message"
            exit 0
            ;;
        *)
            echo "Unknown option: $1"
            echo "Usage: $0 [-a|--all] | [-c|--changed] [-y|--yes]"
            ;;
    esac
done

# If no options provided, default to checking changed files against master branch
if [ -z "$SOURCE_FILES" ]; then
  SOURCE_FILES=$(git diff --name-only origin/master 2> /dev/null | grep -E '\.(cpp|h|hpp|c)$')
fi

# If still no source files found, exit with message
if [ -z "$SOURCE_FILES" ]; then
  echo "No source files found to check. Exiting."
  exit 0
fi

# Run dos2unix on all project files to ensure consistent line endings before running uncrustify
echo "Converting line endings to Unix format for all project files."
find . -type f \( -name "*.cpp" -o -name "*.h" -o -name "*.hpp" -o -name "*.c" -o -name "*.cmake" -o -name "CMakeLists.txt" -o -name "*.py" -o -name "*.sh" \) -exec dos2unix {} > /dev/null 2>&1 \;

echo "Using uncrustify configuration from ${UNCRUST_CONFIG}"

# Check each file individually and collect failures
FAILED_FILES=()
FAILED_COUNT=0
TOTAL_FILES=0

for file in $SOURCE_FILES; do
  file=$(echo "$file" | tr -d '\n')
  if [ ! -f "$file" ]; then
    echo "Skipping non-existent file: $file"
    continue
  fi
  TOTAL_FILES=$((TOTAL_FILES + 1))
  if ! uncrustify -c ${UNCRUST_CONFIG} --check -q "$file"; then
    echo "❌ FAILED: $file"
    FAILED_FILES+=("$file")
    FAILED_COUNT=$((FAILED_COUNT + 1))
  else
    echo "✅ PASSED: $file"
  fi
done

if [ $FAILED_COUNT -gt 0 ]; then
  echo -e "\nUncrustify check completed with $FAILED_COUNT failures out of $TOTAL_FILES files checked."
  echo "Failed files:"
  for file in "${FAILED_FILES[@]}"; do
    echo "  - $file"
  done
  if [ "$AUTO_FIX" = true ]; then
    echo "Automatically fixing formatting issues for failed files."
    for file in "${FAILED_FILES[@]}"; do
      echo "Fixing formatting for: $file"
      uncrustify -c ${UNCRUST_CONFIG} --replace --no-backup "$file"
    done
    echo "Formatting fixes applied!"
    exit 1
  else
    echo "Would you like to run uncrustify to fix these issues? (y/n)"
    read -r response
    if [[ "$response" == "y" ]]; then
      for file in "${FAILED_FILES[@]}"; do
        echo "Fixing formatting for: $file"
        uncrustify -c ${UNCRUST_CONFIG} --replace --no-backup "$file"
      done
      echo "Formatting fixes applied!"
    fi
  fi
else
  echo -e "\nUncrustify check completed successfully. All $TOTAL_FILES files passed."
  exit 0
fi