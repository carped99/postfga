# Code Formatting Guide

이 문서는 프로젝트의 코드 포맷팅 설정 및 사용법을 설명합니다.

## 자동 포맷팅 설정

### VSCode 자동 포맷팅

프로젝트에는 다음 포맷팅 설정 파일들이 구성되어 있습니다:

1. **`.clang-format`** - C/C++ 코드 포맷팅 규칙
2. **`.editorconfig`** - 에디터 무관 기본 포맷팅 규칙
3. **`.clang-tidy`** - 코드 품질 및 스타일 체크
4. **`.vscode/settings.json`** - VSCode 특화 설정

### 자동 포맷팅 활성화

VSCode에서 파일을 **저장할 때 자동으로 포맷팅**됩니다:

- **Format On Save**: 활성화됨
- **Format On Paste**: 비활성화됨
- **Format On Type**: 비활성화됨

### 지원 파일 타입

- C 파일 (`.c`, `.h`)
- C++ 파일 (`.cpp`, `.hpp`, `.cc`, `.hh`)
- JSON 파일 (`.json`)
- Markdown 파일 (`.md`)

## 수동 포맷팅

### 단일 파일 포맷팅

#### VSCode에서
1. 파일을 열고 `Shift+Alt+F` (Windows/Linux) 또는 `Shift+Option+F` (Mac)
2. 우클릭 → "Format Document"

#### 커맨드라인에서
```bash
clang-format -i -style=file src/your_file.cpp
```

### 전체 프로젝트 포맷팅

#### 스크립트 사용
```bash
./scripts/format-all.sh
```

#### VSCode Task 사용
1. `Ctrl+Shift+P` (Windows/Linux) 또는 `Cmd+Shift+P` (Mac)
2. "Tasks: Run Task" 선택
3. "format-all" 선택

## 포맷팅 규칙

### 기본 스타일
- **Base Style**: LLVM
- **Standard**: C++20
- **Indentation**: 4 spaces (탭 사용 안 함)
- **Line Length**: 120 characters

### 중괄호 스타일
```cpp
// Allman style
void function()
{
    if (condition)
    {
        // code
    }
    else
    {
        // code
    }
}
```

### 포인터 & 레퍼런스
```cpp
int* ptr;        // Left aligned
int& ref;        // Left aligned
std::string* s;  // Left aligned
```

### 함수 선언
```cpp
// 함수 파라미터는 각 줄에 하나씩
void long_function_name(
    int parameter1,
    const std::string& parameter2,
    double parameter3
);
```

### include 정렬
Include 파일은 다음 순서로 자동 정렬됩니다:

1. PostgreSQL 헤더 (`postgres.h`, `pg_*`)
2. 프로젝트 헤더 (`postfga/*`, `client/*`, etc.)
3. gRPC/protobuf 헤더
4. 외부 라이브러리 (asio 등)
5. C++ 표준 라이브러리
6. C 표준 라이브러리

예시:
```cpp
#include "postgres.h"        // PostgreSQL 메인 헤더

#include <pg_config.h>       // PostgreSQL 헤더

#include "postfga.h"         // 프로젝트 헤더
#include "client/client.hpp"

#include <grpcpp/grpcpp.h>   // gRPC

#include <asio.hpp>          // 외부 라이브러리

#include <memory>            // C++ 표준 라이브러리
#include <string>
#include <vector>

#include <stdio.h>           // C 표준 라이브러리
```

### 생성자 초기화 리스트
```cpp
class Example
{
public:
    Example(int a, int b)
        : member_a_(a)
        , member_b_(b)
        , member_c_(0)
    {
    }

private:
    int member_a_;
    int member_b_;
    int member_c_;
};
```

## Code Linting (clang-tidy)

### 활성화된 검사 항목
- **bugprone-\*** - 버그 가능성 검사
- **cert-\*** - CERT 보안 코딩 규칙
- **clang-analyzer-\*** - Clang static analyzer
- **concurrency-\*** - 동시성 문제 검사
- **cppcoreguidelines-\*** - C++ Core Guidelines
- **modernize-\*** - 현대적 C++ 관행
- **performance-\*** - 성능 최적화
- **readability-\*** - 가독성 향상

### 수동 실행
```bash
clang-tidy src/your_file.cpp -- -I/usr/include/postgresql/18/server
```

## 명명 규칙

### 클래스/구조체/Enum
```cpp
class OpenFgaClient {};     // CamelCase
struct AclCacheEntry {};    // CamelCase
enum class RequestType {};  // CamelCase
```

### 함수
```cpp
void process_request();     // lower_case
bool is_valid();           // lower_case
```

### 변수
```cpp
int request_count;         // lower_case
std::string object_type;   // lower_case
```

### 멤버 변수
```cpp
class Example
{
private:
    int member_var_;       // lower_case with trailing underscore
    std::string name_;     // lower_case with trailing underscore
};
```

### 상수 & 매크로
```cpp
const int MAX_RETRIES = 3;           // UPPER_CASE
constexpr size_t BUFFER_SIZE = 1024; // UPPER_CASE
#define FGA_VERSION "1.0.0"          // UPPER_CASE
```

### 네임스페이스
```cpp
namespace fga::client {}  // lower_case
```

## Git Pre-commit Hook (옵션)

자동으로 커밋 전에 포맷팅하려면:

```bash
cat > .git/hooks/pre-commit << 'EOF'
#!/bin/bash
# Format staged C/C++ files before commit

files=$(git diff --cached --name-only --diff-filter=ACMR | grep -E '\.(c|h|cpp|hpp|cc|hh)$')

if [ -n "$files" ]; then
    echo "Formatting staged files..."
    echo "$files" | xargs clang-format -i -style=file
    echo "$files" | xargs git add
    echo "✅ Formatted files added back to staging"
fi
EOF

chmod +x .git/hooks/pre-commit
```

## 트러블슈팅

### clang-format이 작동하지 않는 경우

1. clang-format 설치 확인:
```bash
which clang-format
clang-format --version
```

2. VSCode 확장 확인:
   - "C/C++" 확장이 설치되어 있는지 확인
   - `ms-vscode.cpptools` 확장 활성화

3. 설정 확인:
   - `.vscode/settings.json`에서 `C_Cpp.formatting` 설정 확인
   - `"C_Cpp.clang_format_style": "file"` 설정 확인

### 특정 코드 블록 포맷팅 무시

```cpp
// clang-format off
int unformatted_array[] = {
    1, 2, 3,
    4, 5, 6
};
// clang-format on
```

## 추가 정보

- [clang-format Documentation](https://clang.llvm.org/docs/ClangFormat.html)
- [C++ Core Guidelines](https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines)
- [EditorConfig](https://editorconfig.org/)
