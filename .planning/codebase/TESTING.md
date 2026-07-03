---
last_mapped_commit: 35507476cbaa482c1c9a6e3582f8475a9f39304c
---

# Testing Patterns

**Analysis Date:** 2026-07-03

## Test Framework

**Runner:** Google Test (GTest), via CMake's `enable_testing()` + `ctest`.

- GTest is configured in `cmake/CommonBuildParameters.cmake:36-38` and `build/CommonCompilerOptions.cmake:13` (`BUILD_TESTING ON`)
- GTest package is found via `find_package(GTest CONFIG REQUIRED)` at `cmake/CommonBuildParameters.cmake:38`
- GTest includes `GTest::gtest_main` and `GTest::gmock_main` for main-function auto-generation

**Custom test helper:** The `addtest()` CMake function in `cmake/functions.cmake:8-31` wraps test creation:
```cmake
function(addtest test_name)
    add_executable(${test_name} ${ARGN})
    addtest_part(${test_name} ${ARGN})
    target_link_libraries(${test_name}
      GTest::gtest_main
      GTest::gmock_main
    )
    file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/xunit)
    set(xml_output "--gtest_output=xml:${CMAKE_BINARY_DIR}/xunit/xunit-${test_name}.xml")
    add_test(NAME ${test_name} COMMAND $<TARGET_FILE:${test_name}> ${xml_output})
    set_target_properties(${test_name} PROPERTIES
      RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/test_bin
      ARCHIVE_OUTPUT_PATH ${CMAKE_BINARY_DIR}/test_lib
      LIBRARY_OUTPUT_PATH ${CMAKE_BINARY_DIR}/test_lib
    )
    disable_clang_tidy(${test_name})
endfunction()
```

**Key behaviors of `addtest()`:**
- Links every test against `GTest::gtest_main` + `GTest::gmock_main`
- Enables XML output to `build/xunit/xunit-<testname>.xml` (CI-compatible)
- Places binaries in `test_bin/`, libraries in `test_lib/`
- Explicitly disables clang-tidy for test targets (tests don't need full static analysis)

**Run Commands:**
```bash
ctest                        # Run all tests (from build directory)
ctest -R <pattern>           # Run tests matching pattern
ctest --output-on-failure    # Show output for failing tests
ctest -N                     # List tests without running
```

## Test File Organization

**Location:** `test/` directory at project root, configured via `test/CMakeLists.txt`.

**Naming pattern:**
- Test source files: `<Component>Test.cpp` (e.g., `TransactionDataTest.cpp`, `GeniusSDKTest.cpp`)
- Test target names match source filenames (e.g., `TransactionDataTest`, `TransactionBlocksTest`, `GeniusSDKTest`)

**Registered tests** (from `test/CMakeLists.txt`):

| Test Target | Source File | Linked Library |
|------------|-------------|----------------|
| `TransactionDataTest` | `TransactionDataTest.cpp` | `GeniusSDK` |
| `TransactionBlocksTest` | `TransactionBlocksTest.cpp` | `GeniusSDK` |
| `GeniusSDKTest` | `GeniusSDKTest.cpp` | `GeniusSDK` |

**Note:** The test source files were not present at scan time. The `test/CMakeLists.txt` references them but they may exist as generated files, exist in a different branch, or be pending creation. The CMake configuration is the source of truth for what tests are expected.

**Structure:**
```
test/
├── CMakeLists.txt              # Test target definitions
├── TransactionDataTest.cpp     # Unit tests for transaction data handling
├── TransactionBlocksTest.cpp   # Unit tests for transaction block operations
└── GeniusSDKTest.cpp           # Integration tests for the SDK public API
```

## Test Build Configuration

**Opt-in via CMake:** Tests are not built by default in the SDK-level CMake configuration. The `CommonBuildParameters.cmake` root-level CMake sets `option(TESTING "Build tests" OFF)` at line 443. Tests are enabled via:

```bash
cmake .. -DTESTING=ON
```

When `TESTING=ON`:
- `enable_testing()` is called (`cmake/CommonBuildParameters.cmake:454`)
- The `test/` subdirectory is built
- Each test target links against `GeniusSDK` library

## Test Structure (Expected Patterns)

Based on the CMake setup and GTest conventions, tests should follow this pattern:

```cpp
#include <gtest/gtest.h>
#include "GeniusSDK.h"

TEST(TransactionDataTest, BasicInitialization) {
    // Arrange: set up test fixtures
    // Act: call SDK function
    // Assert: verify results
}

TEST(GeniusSDKTest, InitWithKeyReturnsNonNull) {
    const char *result = GeniusSDKInitWithKey("./", "dead...", true, true, 40001, false);
    // ...
}
```

**Patterns expected from the CMake configuration:**
- Tests use GTest's `TEST()` macro (no fixture class needed for simple cases based on naming)
- Tests link `GeniusSDK` statically and call SDK C API directly
- No separate test fixture files detected — fixtures are inline in test `.cpp` files

## Mocking

**Framework:** Google Mock (GMock), available via `GTest::gmock_main` linked to every test target by `addtest()` (`cmake/functions.cmake:13`).

**What to mock:** Given the SDK's architecture (a thin C wrapper over C++ internals), mocking should target:
- The underlying `sgns::GeniusNode` class (though it's accessed through a `shared_ptr` in the global namespace at `src/GeniusSDK.cpp:182`)
- The `ProcessingManager`, `Blockchain`, and other `SuperGenius` components

**Mocking approach:** Since the public API functions are free functions (not methods on a class), tests would:
1. Create mock implementations of the `GeniusNode` interface
2. Assign them to the internal `GeniusNodeInstance` pointer before calling API functions
3. OR test through the real initialized SDK (integration-style)

**Note:** The test source files were not available for analysis, so actual mocking patterns cannot be confirmed. The CMake configuration confirms GMock is available and linked.

## Fixtures and Factories

Based on the test naming in `test/CMakeLists.txt`:
- `TransactionDataTest` — likely tests data structures/parsing for transaction data
- `TransactionBlocksTest` — likely tests block-level operations
- `GeniusSDKTest` — likely tests the public SDK API end-to-end

**Test data location:** Not detected. A `sample.json` is referenced in examples (`example/SDKExample.cpp:382`) but no test data directory was found under `test/`. Test fixtures may be sourced from the `example/dev_config.json` (`example/CMakeLists.txt:13-16` copies it to the build) or created inline.

## Coverage

**Configuration:** No explicit code coverage tooling detected (no `gcov`, `lcov`, `llvm-cov`, or `--coverage` flag in CMake). Coverage is not currently configured.

**CI:** The GitHub Actions workflow at `.github/workflows/build-release-tag.yml` builds the SDK across all platforms but does **not** run tests or generate coverage reports. The workflow only produces release artifacts.

**To enable coverage:**
```bash
cmake .. -DCMAKE_CXX_FLAGS="--coverage" -DCMAKE_C_FLAGS="--coverage" -DTESTING=ON
cmake --build .
ctest
# Then generate report with lcov/gcovr
```

## CI Test Integration

**Current state:** Tests are not run in CI. The `.github/workflows/build-release-tag.yml` workflow:
- Builds only (Debug and Release) across Linux, Windows, macOS, Android, iOS
- Does not invoke `ctest` or any test runner
- Does not have a non-tag workflow (no PR checks, no push-to-develop checks)

**Gap:** There is no CI step that runs `cmake --build . --target test` or `ctest`. Tests can only be run locally.

## Test Types

**Unit Tests:** `TransactionDataTest`, `TransactionBlocksTest` — test specific components in isolation. These test internal data structures and logic.

**Integration Tests:** `GeniusSDKTest` — tests the full SDK C API, likely requiring an initialized node. May need a running blockchain backend or mocked `GeniusNode`.

**E2E Tests:** Not used. No end-to-end framework (no Selenium/Playwright/Cypress) detected — this is a C/C++ SDK library.

## Common GTest Patterns (Standard for This Project)

**Test naming:**
- Test suite: `PascalCase` matching component name (e.g., `GeniusSDKTest`)
- Test case: Descriptive `PascalCase` or `snake_case`

**XML output:** All tests produce JUnit-compatible XML at `build/xunit/xunit-<testname>.xml`

**Binary output location:** Tests compile to `build/<platform>/<build-type>/test_bin/`

**Clang-tidy exemption:** Test files are not linted with clang-tidy (the `addtest()` function calls `disable_clang_tidy()`). This is appropriate since test code often uses patterns (macros, global state) that would trigger clang-tidy warnings.

## Sanitizer Testing

**Supported:** Via `-DSANITIZE_CODE=<sanitizer>` at CMake configure time (`build/CommonCompilerOptions.cmake:28-42`). Supports address, undefined behavior, thread, memory sanitizers depending on compiler. No CI configuration currently uses this flag.

---

*Testing analysis: 2026-07-03*
