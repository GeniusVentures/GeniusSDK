# Codebase Structure

**Analysis Date:** 2026-06-08

## Directory Layout

```
GeniusSDK/                           # Project root
├── .clang-format                    # Code formatting rules (Microsoft-based, 4-space, Allman)
├── .clang-tidy                      # Static analysis checks (bugprone, modernize, performance, etc.)
├── .clangd                          # Language server config
├── .gitignore                       # Build artifacts, secrets, IDE files
├── .gitmodules                      # Submodule: build -> ../cmaketemplate
├── Readme.md                        # Build instructions per platform
├── AgentDocs/                       # Agent instruction files
│   └── CLAUDE.md
├── cmake/                           # Project-level CMake (5 files)
│   ├── CommonBuildParameters.cmake  # Central dependency config (593 lines, 40+ find_package calls)
│   ├── CompilationFlags.cmake       # Compiler warning flags (46 lines)
│   ├── functions.cmake              # Project helper functions: addtest, add_flag, geniussdk_install
│   ├── config.cmake.in              # Package config template
│   ├── GeniusSDK_Info_Mac.plist     # macOS bundle Info.plist
│   └── GeniusSDK_Info_ios.plist     # iOS framework Info.plist
├── build/                           # Build infrastructure (git submodule from cmaketemplate)
│   ├── README.md                    # Build template documentation
│   ├── apple.toolchain.cmake        # Apple platform toolchain (ios-cmake fork, 1182 lines)
│   ├── CommonBuildParameters.cmake  # Thin include bridge -> ../../cmake/CommonBuildParameters.cmake
│   ├── CommonCompilerOptions.cmake  # C++17 standard, thirdparty/zkLLVM discovery + auto-download (240 lines)
│   ├── CompilationFlags.cmake       # Per-platform compiler flags (symlink/copy template)
│   ├── cmake/                       # Build helper modules
│   │   ├── functions.cmake          # get_third_party_dir, addtest_mock, compile_proto_to_cpp, whole-archive linking
│   │   ├── definition.cmake         # ADD_COMPILE_FLAG, ADD_CXX_FLAG, ADD_LINK_FLAG, NDEBUG scrubbing
│   │   ├── install.cmake            # project_install, project_install_setup
│   │   ├── print.cmake              # print(), fatal_error() macros
│   │   ├── compile_option_by_platform/
│   │   │   └── Windows.cmake        # MSVC-specific flags (/wd4146, /WX-, /O2, stack size)
│   │   └── toolchain/
│   │       └── cxx17.cmake          # C++17 standard enforcement (3 lines)
│   ├── OSX/                         # macOS build directory
│   │   ├── CMakeLists.txt           # Platform config: MAC_UNIVERSAL, deployment target 13.0
│   │   ├── Debug/                   # Debug build artifacts (populated after cmake)
│   │   └── Release/                 # Release build artifacts (populated after cmake)
│   ├── Linux/                       # Linux build directory
│   │   └── CMakeLists.txt           # Architecture detection (x86_64/aarch64), ABI subfolder
│   ├── Windows/                     # Windows build directory
│   │   └── CMakeLists.txt           # MSVC runtime config, Win32 libraries (wsock32, crypt32, userenv)
│   ├── iOS/                         # iOS build directory
│   │   ├── CMakeLists.txt           # Platform OS64, deployment target 15.0, static libs
│   │   ├── Debug/                   # Debug build artifacts
│   │   └── RelWithDebInfo/          # RelWithDebInfo build artifacts
│   └── Android/                     # Android build directory
│       └── CMakeLists.txt           # NDK toolchain, API level 28, ABI defaults, Boost clang compiler
├── src/                             # Source code (2 files)
│   ├── CMakeLists.txt               # Build targets: GeniusSDK (static), GeniusSDK_shared (shared), framework/bundle
│   ├── GeniusSDK.h                  # Public C API header (345 lines)
│   └── GeniusSDK.cpp                # Implementation (21961 lines)
├── example/                         # Example executables (6 files)
│   ├── CMakeLists.txt               # Three executable targets with whole-archive linking
│   ├── SDKExample.cpp               # Full SDK example
│   ├── SDKIdleExample.cpp           # Idle/minimal example
│   ├── SDKExampleCredentials.cpp    # Credentials-based init example
│   └── dev_config.json              # Dev configuration for examples
└── test/                            # Test sources (1 file)
    └── CMakeLists.txt               # Three test targets (test source files not checked in)
```

## Directory Purposes

**`cmake/`:**
- Purpose: Project-level CMake configuration that is unique to GeniusSDK (not shared via the cmaketemplate submodule)
- Contains: Dependency configuration, compiler flags, helper functions, packaging templates, Apple plists
- Key files: `CommonBuildParameters.cmake` (all third-party dependency config), `functions.cmake` (addtest, geniussdk_install), `CompilationFlags.cmake` (compiler warnings)

**`build/`:**
- Purpose: Cross-platform build infrastructure, shared across GeniusNetwork projects via the `GeniusVentures/cmaketemplate` git submodule
- Contains: Platform CMakeLists, common compiler options, toolchains, helper functions, thirdparty auto-download logic
- Key files: `CommonCompilerOptions.cmake` (C++17, thirdparty/zkLLVM discovery and download), `apple.toolchain.cmake` (Apple cross-compile), platform `CMakeLists.txt` files

**`src/`:**
- Purpose: Library source code — the entire SDK implementation
- Contains: Single public header and single implementation file
- Key files: `GeniusSDK.h` (C API), `GeniusSDK.cpp` (implementation), `CMakeLists.txt` (4 library targets)

**`example/`:**
- Purpose: Standalone executables demonstrating SDK usage patterns
- Contains: Three example programs with whole-archive linking to ensure all SDK symbols are pulled in
- Key files: `SDKExample.cpp`, `SDKIdleExample.cpp`, `SDKExampleCredentials.cpp`, `CMakeLists.txt`

**`test/`:**
- Purpose: Unit tests
- Contains: CMakeLists.txt referencing test source files (test `.cpp` files appear to not be committed in the current checkout)
- Key files: `CMakeLists.txt`

**`AgentDocs/`:**
- Purpose: Agent instruction files for AI-assisted development
- Contains: `CLAUDE.md` with project-specific instructions

## Key File Locations

**Entry Points (Build):**
- `build/OSX/CMakeLists.txt`: macOS build entry
- `build/Linux/CMakeLists.txt`: Linux build entry
- `build/Windows/CMakeLists.txt`: Windows build entry
- `build/iOS/CMakeLists.txt`: iOS build entry
- `build/Android/CMakeLists.txt`: Android build entry

**Entry Points (Code):**
- `src/GeniusSDK.h`: All public API functions (C linkage)

**Configuration:**
- `cmake/CommonBuildParameters.cmake`: All 40+ third-party dependency find_package calls
- `build/CommonCompilerOptions.cmake`: C++ standard, thirdparty/zkLLVM discovery, sanitizer support
- `.clang-format`: Code formatting rules
- `.clang-tidy`: Static analysis checks
- `build/cmake/compile_option_by_platform/Windows.cmake`: Windows-specific compiler flags

**Core Logic:**
- `src/GeniusSDK.cpp`: All SDK implementation (init, balance, mint, transfer, processing, transactions)

**Examples:**
- `example/SDKExample.cpp`: Full SDK usage demonstration
- `example/SDKIdleExample.cpp`: Minimal SDK initialization
- `example/SDKExampleCredentials.cpp`: Credentials-based initialization

**Testing:**
- `test/CMakeLists.txt`: Test target definitions
- Test source files (`.cpp`): Referenced in `test/CMakeLists.txt` but not currently committed

**Build Helpers:**
- `build/cmake/functions.cmake`: `get_third_party_dir()`, `addtest()`, `compile_proto_to_cpp()`, `add_proto_library()`, `TARGET_LINK_LIBRARIES_WHOLE_ARCHIVE()`
- `cmake/functions.cmake`: `addtest()`, `addtest_part()`, `geniussdk_install()`, `disable_clang_tidy()`
- `build/cmake/definition.cmake`: `ADD_COMPILE_FLAG()`, `ADD_CXX_FLAG()`, `ADD_LINK_FLAG()`, `ADD_DEBUG_COMPILE_FLAG()`, `ADD_NONDEBUG_COMPILE_FLAG()`

## Naming Conventions

**Files:**
- CMake modules: PascalCase or snake_case with `.cmake` extension (`CommonBuildParameters.cmake`, `functions.cmake`)
- Source files: PascalCase matching class/target name (`GeniusSDK.cpp`, `GeniusSDK.h`)
- Example files: PascalCase with SDK prefix (`SDKExample.cpp`, `SDKIdleExample.cpp`)
- Test files: PascalCase with Test suffix (`GeniusSDKTest.cpp`)
- Config files: lowercase with hyphens (`.clang-format`, `.clang-tidy`)

**Directories:**
- Platform build directories: OS name proper case (`OSX`, `Linux`, `Windows`, `iOS`, `Android`)
- Build type directories: PascalCase (`Debug`, `Release`, `RelWithDebInfo`)
- Module directories: lowercase (`cmake`, `src`, `example`, `test`, `build`)

**CMake Targets:**
- Library targets: PascalCase (`GeniusSDK`, `GeniusSDK_shared`, `GeniusSDK_bundle`, `GeniusSDK_framework`)
- Example targets: PascalCase with SDK prefix (`SDKExample`, `SDKIdleExample`, `SDKExampleCredentials`)
- Test targets: PascalCase (`TransactionDataTest`, `TransactionBlocksTest`, `GeniusSDKTest`)
- Third-party imported targets: namespace::name (`protobuf::libprotobuf`, `crypto3::algebra`, `sgns::genius_node`)

**CMake Variables:**
- Cache variables: UPPER_SNAKE_CASE (`THIRDPARTY_BUILD_DIR`, `SUPERGENIUS_DIR`, `ZKLLVM_BUILD_DIR`, `BOOST_VERSION`)
- Internal variables: mixed with underscores (`_THIRDPARTY_BUILD_DIR`, `_BOOST_ROOT`, `_PLATFORM`)
- Options: UPPER_SNAKE_CASE (`BUILD_SHARED_LIBS`, `TESTING`, `BUILD_EXAMPLES`)

## Where to Add New Code

**New Public API Function:**
- Declaration: Add to `src/GeniusSDK.h` within the `GNUS_EXPORT_BEGIN`/`GNUS_EXPORT_END` block, mark with `GNUS_VISIBILITY_DEFAULT`
- Implementation: Add to `src/GeniusSDK.cpp`
- If the function introduces significant new logic (>200 lines), create a new `.cpp` file under `src/` and add it to `src/CMakeLists.txt`

**New Third-Party Dependency:**
- Add `find_package` configuration to `cmake/CommonBuildParameters.cmake` (following the THIRDPARTY_BUILD_DIR/DIR pattern)
- Add the include directory to the `GENIUSSDK_THIRDPARTY_INCLUDE_DIRS` list in `cmake/CommonBuildParameters.cmake` for SDK package install
- Ensure the dependency is built in the thirdparty repository first

**New Example:**
- Create new `.cpp` file in `example/`
- Add `add_executable` + `target_link_libraries` block to `example/CMakeLists.txt` (follow the whole-archive linking pattern)
- Copy `dev_config.json` in POST_BUILD if needed

**New Test:**
- Create test `.cpp` file in `test/`
- Add `addtest(TestName TestFile.cpp)` to `test/CMakeLists.txt`
- Link against `GeniusSDK` target
- Google Test conventions: descriptive test names, `addtest()` helper handles gtest_main linking and xUnit output

**New Build Platform:**
- Create `build/<PlatformName>/CMakeLists.txt` following the pattern of existing platforms
- Must include: `include(../cmake/functions.cmake)`, `get_default_root()`, `project()`, `include(../CommonCompilerOptions.cmake)`, `include(../CommonBuildParameters.cmake)`

## Special Directories

**`build/`:**
- Purpose: Build infrastructure shared across GeniusNetwork projects via git submodule
- Generated: No (committed as submodule)
- Committed: Yes (as submodule pointer to `GeniusVentures/cmaketemplate`)

**`build/<Platform>/<BuildType>/`:**
- Purpose: Platform-specific build output directories (created by cmake configure step)
- Generated: Yes
- Committed: No (in `.gitignore` under `build/` for the subdirectory pattern)

**`build/<Platform>/<BuildType>/<ProjectName>/`:**
- Purpose: Install staging directory (`CMAKE_INSTALL_PREFIX` defaults to `${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}`)
- Generated: Yes (by cmake --install)
- Committed: No

---

*Structure analysis: 2026-06-08*
