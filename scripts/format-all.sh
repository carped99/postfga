#!/bin/bash
# 전체 프로젝트 C/C++ 파일 포맷팅 스크립트

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"

cd "$PROJECT_ROOT"

# clang-format 확인
if ! command -v clang-format &> /dev/null; then
    echo "Error: clang-format not found. Please install clang-format."
    exit 1
fi

echo "🔍 Formatting C/C++ files with clang-format..."
echo ""

# 포맷팅할 파일 찾기
find src include \
    -type f \
    \( -name "*.c" -o -name "*.h" -o -name "*.cpp" -o -name "*.hpp" -o -name "*.cc" -o -name "*.hh" \) \
    -not -path "*/build/*" \
    -not -path "*/.git/*" \
    -not -path "*/node_modules/*" \
    -print0 | while IFS= read -r -d '' file; do

    echo "  Formatting: $file"
    clang-format -i -style=file "$file"
done

echo ""
echo "✅ Formatting complete!"
echo ""
echo "📝 Formatted files summary:"
find src include \
    -type f \
    \( -name "*.c" -o -name "*.h" -o -name "*.cpp" -o -name "*.hpp" -o -name "*.cc" -o -name "*.hh" \) \
    -not -path "*/build/*" \
    -not -path "*/.git/*" \
    -not -path "*/node_modules/*" | wc -l | xargs echo "  Total files:"
