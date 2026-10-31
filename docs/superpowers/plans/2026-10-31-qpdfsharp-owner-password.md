# QpdfSharp Owner-Password Protection Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking. Tasks are sequential: each one assumes every earlier task is merged to `main`.

**Goal:** Take this empty repository to a NuGet-packaged .NET library, laid out like SkiaSharp, that uses a native qpdf build to protect a PDF with an owner password, with CI that builds, tests and packs every supported platform.

**Architecture:** Mirrors `mono/SkiaSharp`:
- Upstream qpdf is a git submodule in `externals/`.
- A small C ABI shim (our equivalent of SkiaSharp's `sk_*` C API) links qpdf, zlib and libjpeg-turbo statically into one self-contained `libQpdfSharp` per platform. Platform-specific build files live under `native/<platform>/`.
- The managed library `binding/QpdfSharp` calls the shim through `LibraryImport`. A version gate in the static constructor of the P/Invoke class runs before the first native call.
- The `binding/QpdfSharp.NativeAssets.*` packages ship the binaries under `runtimes/<rid>/native/`.

**Tech Stack:** qpdf 12.4.2 (C++20), CMake ≥ 3.25 + Ninja, vcpkg (zlib, libjpeg-turbo), C#/.NET 8 + 10, xUnit v3 on Microsoft.Testing.Platform, GitHub Actions.

**Spec:** The original request: *"Using SkiaSharp as a good standard, create a list of tasks to take this from an empty repo to a library with native qpdf bindings. The only functionality of this library (for now) is to protect it with an owner password. We will need pipelines to test this as well."* Publishing to nuget.org was later ruled out of scope. The **Design Decisions** section below turns the request into concrete decisions. Executors read both.

## Target layout (mirrors SkiaSharp)

```
qpdfsharp/
├─ AGENTS.md                         # layout + build/test commands for agents (SkiaSharp has one too)
├─ External-Dependency-Info.txt      # third-party licenses; packed as THIRD-PARTY-NOTICES.txt
├─ LICENSE.md  README.md  global.json  Directory.Build.props  Directory.Packages.props  QpdfSharp.slnx
├─ externals/qpdf/                   # submodule @ v12.4.2
├─ native/
│  ├─ libQpdfSharp/                  # shared C ABI shim + CMake project (SkiaSharp keeps its C API in its skia fork)
│  │  ├─ CMakeLists.txt  vcpkg.json  triplets/
│  │  ├─ include/qpdfsharp.h  src/qpdfsharp.cpp  tests/
│  ├─ linux/   build.sh  check.sh  libQpdfSharp/libQpdfSharp.map
│  ├─ macos/   build.sh  check.sh  libQpdfSharp/libQpdfSharp.exports
│  └─ windows/ build.ps1 check.ps1
├─ binding/
│  ├─ Directory.Build.targets        # imports NativeAssets.Build.targets for *.NativeAssets.* projects
│  ├─ NativeAssets.Build.targets
│  ├─ IncludeNativeAssets.QpdfSharp.targets   # copies output/native/... into test output
│  ├─ QpdfSharp/                     # managed library (QpdfApi.cs, QpdfSharpVersion.cs, PACKAGE-README.md, …)
│  ├─ QpdfSharp.NativeAssets.Linux/
│  ├─ QpdfSharp.NativeAssets.macOS/
│  └─ QpdfSharp.NativeAssets.Win32/
├─ tests/
│  ├─ Content/pdfs/                  # shared test assets (native and managed tests both use these)
│  └─ QpdfSharp.Tests/
├─ samples/Basic/Console/QpdfSharpSample/   # consumes the packed .nupkg files
├─ scripts/  pack.sh  run-sample.sh
├─ .github/workflows/ci.yml
└─ output/  build/                   # git-ignored
   └─ output/native/{windows/<arch>, linux/<arch>, osx}/libQpdfSharp.*   (same layout as SkiaSharp)
```

## Design Decisions

These are fixed. Do not change them without asking the user.

1. **Own C shim, not raw qpdf exports.** We do not export qpdf's ~400 C/C++ symbols. Instead `libQpdfSharp` exports exactly five `qpdfsharp_*` functions (header given in Tasks 1–2). This follows SkiaSharp's `libSkiaSharp`, which exports only `sk_*`/`gr_*` via `native/linux/libSkiaSharp/libSkiaSharp.map`. The shim uses qpdf's C++ API (`QPDF`, `QPDFWriter`) and turns every exception into a status code. No exception may cross the C boundary.
2. **Native/managed compatibility gate**, as SkiaSharp does with `SkiaApi`'s static constructor and `SkiaSharpVersion.CheckNativeLibraryCompatible`. The shim exports `qpdfsharp_get_abi_version()`, which returns `QPDFSHARP_ABI_VERSION` (currently `1`). The managed `QpdfApi` static constructor checks it before any other native call. On a mismatch it throws `InvalidOperationException`, which the runtime caches as a `TypeInitializationException`. Bump the ABI version whenever the C header changes incompatibly.
3. **"Owner password only" means:** the user password is empty, so anyone can open the file, and the owner password is required to lift the restrictions. Encryption is always R6 (AES-256) via `QPDFWriter::setR6EncryptionParameters`, with `encrypt_metadata_aes = true`. By default no permissions are granted. Callers opt in through `PdfPermissions` flags.
4. **No accessibility flag.** qpdf always allows accessibility for R > 3 (`QPDFWriter.cc`: "There is no way to disable accessibility with R > 3"). The shim always passes `allow_accessibility = true`.
5. **Password encoding:** the managed side normalises to Unicode NFKC (an approximation of the SASLprep that PDF 2.0 requires for R6) and then encodes as UTF-8. It rejects passwords whose UTF-8 form exceeds 127 bytes (R6 silently truncates at 127) and passwords that contain U+0000.
6. **Platforms and binaries, laid out as SkiaSharp lays them out:**

   | Package | Package path | Local output path | Built by |
   |---|---|---|---|
   | Win32 | `runtimes/win-x64/native/libQpdfSharp.dll` | `output/native/windows/x64/` | `native/windows/build.ps1 -Arch x64` |
   | Win32 | `runtimes/win-arm64/native/libQpdfSharp.dll` | `output/native/windows/arm64/` | `native/windows/build.ps1 -Arch arm64` |
   | Linux | `runtimes/linux-x64/native/libQpdfSharp.so` | `output/native/linux/x64/` | `native/linux/build.sh x64` |
   | Linux | `runtimes/linux-arm64/native/libQpdfSharp.so` | `output/native/linux/arm64/` | `native/linux/build.sh arm64` |
   | macOS | `runtimes/osx/native/libQpdfSharp.dylib` (**universal** arm64 + x86_64, as SkiaSharp ships) | `output/native/osx/` | `native/macos/build.sh` |

   Linux means glibc only (musl/Alpine is out of scope). The managed import name is `"libQpdfSharp"`.
7. **Packages:**
   - `QpdfSharp` (managed) has plain `ProjectReference`s to all three native-asset projects, so they become package dependencies.
   - The native-asset projects are `QpdfSharp.NativeAssets.Win32`, `QpdfSharp.NativeAssets.Linux` and `QpdfSharp.NativeAssets.macOS`, with shared pack logic in `binding/NativeAssets.Build.targets`.
8. **Repository license:** MIT (as SkiaSharp uses). `External-Dependency-Info.txt` holds the licenses of qpdf (Apache-2.0), zlib (zlib License) and libjpeg-turbo (IJG/BSD-3-Clause/zlib). It is packed into every package as `THIRD-PARTY-NOTICES.txt`.

**Deliberate differences from SkiaSharp.** These are simplifications for a single-function library. Each can be revisited later.
- **Where the C API lives.** SkiaSharp keeps its C API inside its fork of Skia (`externals/skia/include/c`, `src/c`). We don't fork qpdf, so our shim lives in `native/libQpdfSharp/`.
- **Build tooling.** SkiaSharp orchestrates builds with Cake (`build.cake`, `native/<platform>/build.cake`) and builds natively with GN/Xcode/vcxproj. We use one CMake project, driven by `native/<platform>/build.sh|ps1`.
- **CI system.** SkiaSharp's main CI is Azure Pipelines (`scripts/azure-*.yml`). We use GitHub Actions, with the same stage order: native → managed/pack → tests.
- **Linux by default.** SkiaSharp does not reference its Linux native-asset package by default. We do, so that one `dotnet add package QpdfSharp` works everywhere.
- **Fewer target frameworks.** We target only `net8.0;net10.0`. That means no `netstandard`/`net4x`/mobile targets, and so none of SkiaSharp's `Binding.Shared/LibraryLoader.cs`, `USE_DELEGATES` path, or `buildTransitive/net4` targets.

## Global Constraints

- qpdf submodule pinned to tag `v12.4.2`. qpdf CMake options: `BUILD_SHARED_LIBS=OFF`, `BUILD_STATIC_LIBS=ON`, `REQUIRE_CRYPTO_NATIVE=ON`, `USE_IMPLICIT_CRYPTO=OFF`, `BUILD_DOC=OFF`, `MAINTAINER_MODE=OFF`, `INSTALL_PKGCONFIG=OFF`, `INSTALL_CMAKE_PACKAGE=OFF`, `INSTALL_EXAMPLES=OFF`. No OpenSSL or GnuTLS anywhere.
- vcpkg pinned to release tag `2026.10.16`: `builtin-baseline` in `native/libQpdfSharp/vcpkg.json` is that tag's commit SHA, and CI clones that tag. Dependencies: `zlib`, `libjpeg-turbo`. Overlay triplets in `native/libQpdfSharp/triplets/` are named `qpdfsharp-<os>-<arch>.cmake` (`os` ∈ `osx|linux|windows`, `arch` ∈ `x64|arm64`). All are static, `VCPKG_BUILD_TYPE release`.
- C++ standard 20.
- **Exported symbols:** exactly `qpdfsharp_get_qpdf_version`, `qpdfsharp_get_abi_version`, `qpdfsharp_protect_with_owner_password`, `qpdfsharp_inspect_encryption` and `qpdfsharp_free_buffer`. Nothing else, on every platform.
- **Runtime dependencies of `libQpdfSharp`:**
  - Linux: only `libc.so.6`, `libm.so.6`, `libpthread.so.0`, `libdl.so.2`, `librt.so.1`, `ld-linux-*.so.*`, `libstdc++.so.6` and `libgcc_s.so.1`. .NET itself requires the last two. Highest `GLIBC_` version referenced ≤ `2.28`. Built inside `quay.io/pypa/manylinux_2_28_<arch>`.
  - macOS: only `/usr/lib/libc++.1.dylib` and `/usr/lib/libSystem.B.dylib`. `minos` ≤ `12.0` for both slices. `lipo -archs` lists `x86_64` and `arm64`.
  - Windows: static CRT (`/MT`, `CMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded`). No `MSVCP*`, `VCRUNTIME*` or `api-ms-win-crt-*` dependents. No zlib/jpeg/qpdf DLL dependents.
- Managed TFMs: `net8.0;net10.0`, for the library and the native-asset projects alike. `<Nullable>enable</Nullable>` and `<TreatWarningsAsErrors>true</TreatWarningsAsErrors>`. `<IsAotCompatible>true</IsAotCompatible>` on the library. Use `LibraryImport`, not `DllImport`. Central package management (`Directory.Packages.props`).
- `global.json`: SDK `10.0.100`, `rollForward: latestFeature`, `"test": { "runner": "Microsoft.Testing.Platform" }`.
- Tests: `xunit.v3` on Microsoft.Testing.Platform (as SkiaSharp's `SkiaSharp.Tests.Console` uses), with `Microsoft.Testing.Extensions.TrxReport`. No VSTest.
- Version: `VersionPrefix` `0.1.0` in `Directory.Build.props`. CI builds append `-ci.<run_number>`. Nothing is published to nuget.org (out of scope for now).
- `output/` and `build/` are git-ignored. Intermediate CMake trees go in `build/native/<os>-<arch>/`. Packages go to `output/nuget/`.
- Root namespace and assembly: `QpdfSharp`. Author: `Mitchell Cash`. Repository: `https://github.com/MitchellCash/qpdfsharp` (public, so GitHub's free arm64 runners `ubuntu-24.04-arm` and `windows-11-arm` are available).

## Review Focus

1. **Passwords outside plain ASCII or outside the R6 limit.** A password typed in decomposed form (`"päss"`) must unlock a file protected with the composed form (`"päss"`). A password longer than 127 UTF-8 bytes, an empty password, or one containing `'\0'` must throw `ArgumentException` before any native call. *Tests: Task 4.*
2. **Input that needs a user password to open.** This must throw `QpdfException` with `Kind == QpdfErrorKind.PasswordRequired`. It must not crash, hang, or return an unprotected file. *Tests: Task 2 (native), Task 4 (managed).*
3. **Input that is already owner-protected.** Re-protecting must succeed. Afterwards the new owner password matches and the old one does not. *Tests: Task 4.*
4. **Many threads protecting at once.** 32 parallel calls must all produce valid, independently verifiable output, which proves the shim has no shared mutable state. *Tests: Task 4.*
5. **Bytes that are not a PDF.** Random bytes must throw `QpdfException` with `Kind == QpdfErrorKind.InvalidPdf`. An empty input must throw `ArgumentException`. *Tests: Task 2 (native), Task 4 (managed).*

---

## Local prerequisites (macOS dev machine; CI installs its own)

```bash
brew install cmake ninja pkg-config nasm qpdf
git clone https://github.com/microsoft/vcpkg ~/vcpkg && git -C ~/vcpkg checkout 2026.10.16 && ~/vcpkg/bootstrap-vcpkg.sh -disableMetrics
export VCPKG_ROOT=~/vcpkg
```

The `qpdf` CLI is needed **only** to generate one committed test asset (Task 2) and as an independent oracle in CI (Task 6). The library never shells out to it.

---

### Task 1: Repository skeleton and a self-contained universal macOS `libQpdfSharp`

The deliverable is `output/native/osx/libQpdfSharp.dylib`, a universal binary with qpdf linked statically that exports only the two version functions, plus scripts that build it and verify its exports, dependencies and architectures.

**Files:**
- Create the root files:
  - `.gitignore` (dotnet template + `build/`, `output/`)
  - `.gitattributes` (`* text=auto`, `*.pdf binary`, `*.sh text eol=lf`)
  - `.editorconfig`
  - `LICENSE.md` (MIT, "Copyright (c) 2026 Mitchell Cash")
  - `External-Dependency-Info.txt` (full license texts of qpdf, zlib and libjpeg-turbo)
  - `README.md` (one-paragraph stub)
  - `AGENTS.md` (the Target layout tree above, the local prerequisites, and the build command; later tasks add their commands)
- Create: `.gitmodules` + submodule `externals/qpdf` at tag `v12.4.2`
- Create: `native/libQpdfSharp/CMakeLists.txt`, `native/libQpdfSharp/vcpkg.json`, `native/libQpdfSharp/triplets/qpdfsharp-osx-arm64.cmake`, `qpdfsharp-osx-x64.cmake`
- Create: `native/libQpdfSharp/include/qpdfsharp.h`, `native/libQpdfSharp/src/qpdfsharp.cpp` (version functions only in this task)
- Create: `native/libQpdfSharp/tests/smoke.c` (registered with CTest)
- Create: `native/linux/libQpdfSharp/libQpdfSharp.map` (GNU ld version script with node `libQpdfSharp { global: qpdfsharp_*; local: *; };`, the same shape as SkiaSharp's map)
- Create: `native/macos/libQpdfSharp/libQpdfSharp.exports` (one `_qpdfsharp_…` name per line)
- Create: `native/macos/build.sh`, `native/macos/check.sh`

**Interfaces:**
- Produces: `native/macos/build.sh`. For each of `arm64` and `x86_64`, it:
  1. Configures `build/native/osx-<arch>` with Ninja, the vcpkg toolchain (`$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake`), `VCPKG_TARGET_TRIPLET=qpdfsharp-osx-<arm64|x64>`, `VCPKG_OVERLAY_TRIPLETS=native/libQpdfSharp/triplets` and `CMAKE_OSX_ARCHITECTURES=<arch>`.
  2. Builds it.
  3. Runs `ctest --output-on-failure` if the slice can execute on this machine. The x86_64 slice runs under Rosetta; skip it with a printed notice if `arch -x86_64 /usr/bin/true` fails.

  It then runs `lipo -create` and writes `output/native/osx/libQpdfSharp.dylib`. It exits non-zero on any failure, or if `VCPKG_ROOT` is unset.
- Produces: `native/macos/check.sh [path]`, defaulting to `output/native/osx/libQpdfSharp.dylib`. It checks each slice (`lipo -thin`) against the Global Constraints:
  - Exports: `nm -gU` must equal the expected list exactly, with a leading `_` on each name.
  - Dependencies: `otool -L` may list only the allowed libraries and the library's own install name `@rpath/libQpdfSharp.dylib`.
  - Deployment target: `otool -l` `minos` ≤ 12.0.
  - Architectures: `lipo -archs` lists both `x86_64` and `arm64`.

  It prints every violation and exits non-zero if there are any, or prints `OK`. Keep the expected export list in one variable at the top of the script. Task 2 widens it.
- Produces: CMake target `QpdfSharp` (SHARED, `PREFIX ""`, `OUTPUT_NAME libQpdfSharp`), compiled with `QPDFSHARP_BUILDING` defined and `CXX_VISIBILITY_PRESET hidden`.
- Produces, in `qpdfsharp.h`:

```c
#define QPDFSHARP_ABI_VERSION 1
QPDFSHARP_API const char* qpdfsharp_get_qpdf_version(void); /* QPDF::QPDFVersion().c_str(); static storage */
QPDFSHARP_API int32_t qpdfsharp_get_abi_version(void);      /* returns QPDFSHARP_ABI_VERSION */
```

- [ ] **Step 1: Scaffold the repo files and add the submodule**

```bash
git submodule add https://github.com/qpdf/qpdf.git externals/qpdf
git -C externals/qpdf checkout v12.4.2
```

- [ ] **Step 2: Write the failing CTest smoke test** in `native/libQpdfSharp/tests/smoke.c`. It asserts:
  - `qpdfsharp_get_qpdf_version()` is non-NULL.
  - `strncmp(v, "12.4.2", 6) == 0`.
  - `qpdfsharp_get_abi_version() == QPDFSHARP_ABI_VERSION`.

  It returns 0 on success and 1 otherwise.

- [ ] **Step 3: Write `vcpkg.json`, the two macOS triplets, and `CMakeLists.txt`**
  - `vcpkg.json`: name `qpdfsharp-native`, dependencies `zlib` and `libjpeg-turbo`, and `builtin-baseline` = `git -C $VCPKG_ROOT rev-parse 2026.10.16^{commit}`.
  - Triplets: `VCPKG_TARGET_ARCHITECTURE`, `VCPKG_CRT_LINKAGE dynamic`, `VCPKG_LIBRARY_LINKAGE static`, `VCPKG_BUILD_TYPE release`, `VCPKG_CMAKE_SYSTEM_NAME Darwin`, `VCPKG_OSX_ARCHITECTURES` and `VCPKG_OSX_DEPLOYMENT_TARGET 12.0`.
  - `CMakeLists.txt`:
    - Set `CMAKE_POSITION_INDEPENDENT_CODE ON`, `CMAKE_MSVC_RUNTIME_LIBRARY MultiThreaded` and `CMAKE_OSX_DEPLOYMENT_TARGET 12.0`.
    - Set the qpdf options from Global Constraints as cache variables, then `add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../../externals/qpdf qpdf EXCLUDE_FROM_ALL)`.
    - Link qpdf's static target into `QpdfSharp`. With shared libs off, the static target is named `libqpdf`. Check `externals/qpdf/libqpdf/CMakeLists.txt` around line 625.
    - Linux: link options `-Wl,--version-script=${CMAKE_CURRENT_SOURCE_DIR}/../linux/libQpdfSharp/libQpdfSharp.map`, `-Wl,--exclude-libs,ALL` and `-Wl,--gc-sections`.
    - Apple: link options `-Wl,-exported_symbols_list,${CMAKE_CURRENT_SOURCE_DIR}/../macos/libQpdfSharp/libQpdfSharp.exports` and `-Wl,-dead_strip`.
    - Add `enable_testing()` and an `add_test` for `smoke`.
  - Known pitfall: qpdf finds zlib/libjpeg via `pkg_check_modules` first and `find_library` second. The vcpkg prefix must win over Homebrew/system copies. `check.sh` catches a dynamic `libz`/`libjpeg` dependency if it doesn't.

- [ ] **Step 4: Write `qpdfsharp.h` and `qpdfsharp.cpp`.** The header's export macro is `__declspec(dllexport)` when `QPDFSHARP_BUILDING` is defined on `_WIN32`, `__declspec(dllimport)` otherwise on `_WIN32`, and `__attribute__((visibility("default")))` elsewhere. Wrap the declarations in `extern "C"` and include `<stddef.h>` and `<stdint.h>`.

- [ ] **Step 5: Write `native/macos/build.sh` and `native/macos/check.sh`.**

- [ ] **Step 6: Build and verify**

Run: `./native/macos/build.sh && ./native/macos/check.sh`
Expected: CTest `smoke ... Passed` for arm64 (and for x86_64 when Rosetta is available); `check.sh` prints `OK`; `lipo -archs output/native/osx/libQpdfSharp.dylib` prints both `x86_64` and `arm64`.

- [ ] **Step 7: Prove the checker catches violations.** Temporarily remove the Apple `-exported_symbols_list` option and rebuild. `check.sh` must exit non-zero and list qpdf C++ symbols. Restore the option.

- [ ] **Step 8: Commit**

```bash
git add -A && git commit -m "build: add universal macOS libQpdfSharp with statically linked qpdf"
```

---

### Task 2: Native shim — protect and inspect

**Files:**
- Modify: `native/libQpdfSharp/include/qpdfsharp.h`, `native/libQpdfSharp/src/qpdfsharp.cpp`, `native/macos/libQpdfSharp/libQpdfSharp.exports`, `native/macos/check.sh` (expected export list → all five)
- Create: `tests/Content/pdfs/minimal.pdf`, `tests/Content/pdfs/user-password.pdf` and `tests/Content/pdfs/README.md` (see Step 1)
- Create: `native/libQpdfSharp/tests/protect_tests.c`
- Modify: `native/libQpdfSharp/CMakeLists.txt` (register the new test, and pass `QPDFSHARP_TEST_CONTENT_DIR="${CMAKE_CURRENT_SOURCE_DIR}/../../tests/Content/pdfs"` as a compile definition)

**Interfaces:**
- Consumes: Task 1's header, target and scripts.
- Produces: these additions to `qpdfsharp.h`. This is the exact contract Task 3 binds to:

```c
typedef enum qpdfsharp_status {
    QPDFSHARP_STATUS_OK = 0,
    QPDFSHARP_STATUS_INVALID_ARGUMENT = 1,
    QPDFSHARP_STATUS_PASSWORD_REQUIRED = 2,
    QPDFSHARP_STATUS_INVALID_PDF = 3,
    QPDFSHARP_STATUS_INTERNAL_ERROR = 4
} qpdfsharp_status;

/* Actions allowed to anyone who opens the file without the owner password. */
#define QPDFSHARP_ALLOW_PRINT_LOW_RESOLUTION  (1u << 0)
#define QPDFSHARP_ALLOW_PRINT_HIGH_RESOLUTION (1u << 1) /* implies low resolution */
#define QPDFSHARP_ALLOW_EXTRACT_CONTENT       (1u << 2)
#define QPDFSHARP_ALLOW_ASSEMBLE_DOCUMENT     (1u << 3)
#define QPDFSHARP_ALLOW_ANNOTATE_AND_FORM     (1u << 4)
#define QPDFSHARP_ALLOW_FILL_FORMS            (1u << 5)
#define QPDFSHARP_ALLOW_MODIFY_CONTENT        (1u << 6)
#define QPDFSHARP_ALLOW_ALL                   (0x7Fu)

typedef struct qpdfsharp_encryption_info {
    int32_t is_encrypted;           /* 0 or 1 */
    int32_t revision;               /* R value; 0 when not encrypted */
    int32_t owner_password_matched; /* 0 or 1 */
    int32_t user_password_matched;  /* 0 or 1 */
    uint32_t permissions;           /* QPDFSHARP_ALLOW_* bits */
} qpdfsharp_encryption_info;

/* On OK, *output is a buffer the caller must release with qpdfsharp_free_buffer.
   On any other status, *output is NULL and *output_length is 0.
   error_message (may be NULL when capacity is 0) always receives a NUL-terminated,
   possibly truncated, UTF-8 message; empty on OK. */
QPDFSHARP_API qpdfsharp_status qpdfsharp_protect_with_owner_password(
    const uint8_t* input, size_t input_length,
    const char* owner_password,          /* NUL-terminated UTF-8, 1..127 bytes */
    uint32_t allowed_permissions,        /* only QPDFSHARP_ALLOW_* bits */
    uint8_t** output, size_t* output_length,
    char* error_message, size_t error_message_capacity);

QPDFSHARP_API qpdfsharp_status qpdfsharp_inspect_encryption(
    const uint8_t* input, size_t input_length,
    const char* password,                /* NUL-terminated UTF-8; "" for none */
    qpdfsharp_encryption_info* info,
    char* error_message, size_t error_message_capacity);

QPDFSHARP_API void qpdfsharp_free_buffer(uint8_t* buffer); /* NULL is a no-op */
```

**Behaviour the implementation must have:**
- **Argument validation → `INVALID_ARGUMENT`:**
  - `input`, `output`, `output_length`, `info` or a password pointer is NULL.
  - `input_length == 0`.
  - The owner password is empty or longer than 127 bytes.
  - `allowed_permissions & ~QPDFSHARP_ALLOW_ALL` is non-zero.
- **Protect:**
  1. Create `QPDF pdf;` and call `pdf.setSuppressWarnings(true)`.
  2. Call `pdf.processMemoryFile("input", input, input_length, "")`. qpdf requires the buffer to outlive the `QPDF` object, which holds because the object is local to the call.
  3. Create `QPDFWriter w(pdf)` and call `w.setOutputMemory()`.
  4. Call `w.setR6EncryptionParameters("", owner, true, extract, assemble, annotate, fill_forms, modify, print, true)`, where `print` is `qpdf_r3p_full` if `PRINT_HIGH_RESOLUTION` is set, else `qpdf_r3p_low` if `PRINT_LOW_RESOLUTION` is set, else `qpdf_r3p_none`.
  5. Call `w.write()` and copy `w.getBufferSharedPointer()` into a `malloc`'d buffer.
- **Inspect:** call `pdf.isEncrypted(R, P)`, `ownerPasswordMatched()` and `userPasswordMatched()`. Build `permissions` from `allowPrintLowRes`, `allowPrintHighRes`, `allowExtractAll`, `allowModifyAssembly`, `allowModifyAnnotation`, `allowModifyForm` and `allowModifyOther`. These map 1:1 onto P bits 3, 12, 5, 11, 6, 9 and 4, the same bits `setR6EncryptionParameters` writes. If the file is not encrypted, `revision = 0` and `permissions = QPDFSHARP_ALLOW_ALL`.
- **Exception mapping:**
  - `QPDFExc` with `getErrorCode() == qpdf_e_password` → `PASSWORD_REQUIRED`.
  - Any other `QPDFExc` → `INVALID_PDF`.
  - `std::exception` or `...` → `INTERNAL_ERROR`.
  - Copy the `what()` text into `error_message`.

- [ ] **Step 1: Create the shared test assets with the qpdf CLI**
  - `minimal.pdf`: hand-write a one-page PDF (Catalog → Pages → one Page with `/MediaBox [0 0 612 792]` and a content stream drawing `BT /F1 12 Tf 72 720 Td (QpdfSharp) Tj ET` with Helvetica), then normalise it with `qpdf minimal-src.pdf tests/Content/pdfs/minimal.pdf` so the xref is correct.
  - `user-password.pdf`: run `qpdf --encrypt --user-password=user-secret --owner-password=owner-secret --bits=256 -- tests/Content/pdfs/minimal.pdf tests/Content/pdfs/user-password.pdf`.
  - Record both commands in `tests/Content/pdfs/README.md`.

- [ ] **Step 2: Write the failing native tests** in `protect_tests.c`, using plain C asserts that print the failing case:
  - `protect_minimal_ok`: status OK; `output_length > 0`; output starts with `%PDF-`.
  - `protect_then_inspect_with_owner`: inspect(output, `"owner-secret"`) gives `is_encrypted == 1`, `revision == 6`, `owner_password_matched == 1`, `permissions == 0`.
  - `protect_then_inspect_without_password`: inspect(output, `""`) gives `is_encrypted == 1`, `owner_password_matched == 0`, `user_password_matched == 1`.
  - `protect_print_high_reports_both_print_bits`: allowed = `PRINT_HIGH_RESOLUTION` → inspected `permissions == (PRINT_LOW_RESOLUTION | PRINT_HIGH_RESOLUTION)`.
  - `protect_each_single_flag_round_trips`: loop over the other six single bits. Each one's inspected `permissions` must equal the flag alone.
  - `protect_user_password_input_is_password_required` (Review Focus 2): input `user-password.pdf` → `PASSWORD_REQUIRED`; `*output == NULL`; `error_message` is non-empty.
  - `protect_garbage_is_invalid_pdf` (Review Focus 5): 4096 bytes of `0xA5` → `INVALID_PDF`.
  - `protect_rejects_bad_arguments`: empty input, NULL password, `""` password, a 128-byte password, and `allowed_permissions = 0x80` → each returns `INVALID_ARGUMENT`.
  - `free_buffer_null_is_noop`.

- [ ] **Step 3: Run the tests to make sure they fail**

Run: `./native/macos/build.sh`
Expected: the build fails because `qpdfsharp_protect_with_owner_password` is undeclared.

- [ ] **Step 4: Implement the header and the shim as specified above.** Update `libQpdfSharp.exports` and the `check.sh` expected list to the five functions.

- [ ] **Step 5: Run the tests and the checks**

Run: `./native/macos/build.sh && ./native/macos/check.sh`
Expected: all CTest cases pass; checker prints `OK` with exactly five exports per slice.

- [ ] **Step 6: Commit**

```bash
git add -A && git commit -m "feat(native): add owner-password protect and inspect C API"
```

---

### Task 3: Managed binding layer and public API

**Files:**
- Create: `global.json`, `Directory.Build.props`, `Directory.Packages.props`, `QpdfSharp.slnx`
- Create: `binding/QpdfSharp/QpdfSharp.csproj`, `binding/QpdfSharp/PACKAGE-README.md`
- Create: `binding/QpdfSharp/QpdfApi.cs`. This is the internal `partial class` holding every `LibraryImport`, with `private const string QPDFSHARP = "libQpdfSharp";` and the static-constructor gate. It is the analogue of SkiaSharp's `SkiaApi.cs`.
- Create: `binding/QpdfSharp/QpdfSharpVersion.cs`, `PdfProtection.cs`, `PdfPermissions.cs`, `QpdfException.cs`, `QpdfErrorKind.cs`, `PdfEncryptionInfo.cs` (internal)

**Interfaces:**
- Consumes: the Task 1–2 C header (exact names, values and struct layout). `qpdfsharp_encryption_info` maps to an internal `[StructLayout(LayoutKind.Sequential)]` struct of four `int` fields and one `uint`.
- Produces (public):

```csharp
namespace QpdfSharp;

[Flags]
public enum PdfPermissions : uint
{
    None = 0,
    PrintLowResolution   = 1u << 0,
    PrintHighResolution  = (1u << 1) | PrintLowResolution,
    ExtractContent       = 1u << 2,
    AssembleDocument     = 1u << 3,
    AnnotateAndFillForms = 1u << 4,
    FillForms            = 1u << 5,
    ModifyContent        = 1u << 6,
    All                  = 0x7F,
}

public enum QpdfErrorKind { PasswordRequired, InvalidPdf, Internal }

public sealed class QpdfException : Exception
{
    public QpdfErrorKind Kind { get; }
    // ctor(QpdfErrorKind kind, string message)
}

public static class PdfProtection
{
    public static string QpdfVersion { get; }
    public static byte[] ProtectWithOwnerPassword(ReadOnlySpan<byte> pdf, string ownerPassword, PdfPermissions allowed = PdfPermissions.None);
    public static void ProtectWithOwnerPassword(string inputPath, string outputPath, string ownerPassword, PdfPermissions allowed = PdfPermissions.None);
}
```

- Produces (internal, visible to tests via `InternalsVisibleTo("QpdfSharp.Tests")`):
  - `internal static class QpdfSharpVersion` with:
    - `internal const int SupportedAbiVersion = 1;`
    - `internal static bool CheckNativeLibraryCompatible(int supported, int native, bool throwIfIncompatible)`. If the versions differ and `throwIfIncompatible` is set, it throws `InvalidOperationException("The native libQpdfSharp library (ABI {native}) is incompatible with this version of QpdfSharp (requires ABI {supported}).")`.
  - `QpdfApi`'s static constructor calls `QpdfSharpVersion.CheckNativeLibraryCompatible(SupportedAbiVersion, qpdfsharp_get_abi_version(), true)`.
  - `internal static PdfEncryptionInfo PdfProtection.InspectEncryption(ReadOnlySpan<byte> pdf, string password)`
  - `internal readonly record struct PdfEncryptionInfo(bool IsEncrypted, int Revision, bool OwnerPasswordMatched, bool UserPasswordMatched, PdfPermissions Permissions)`

**Behaviour:**
- **Validation, in order, before any native call:**
  1. `ArgumentException` if `pdf.IsEmpty`.
  2. `ArgumentNullException` if the password is null.
  3. `ArgumentException` if the password is empty or contains `'\0'`.
  4. Normalise with `NormalizationForm.FormKC`.
  5. `ArgumentException` if the normalised UTF-8 form is longer than 127 bytes.
  6. `ArgumentOutOfRangeException` if `allowed` has bits outside `All`.

  `InspectEncryption` applies the same password rules, except that it allows `""`.
- **Native call:**
  - Encode the password as a NUL-terminated UTF-8 `byte[]` and pass it as `byte*`. Do not use `StringMarshalling`, because the NFKC step must happen first.
  - Keep `pdf` pinned (`fixed`) for the whole call.
  - The error buffer is `stackalloc byte[1024]`.
  - On OK, copy the native buffer into a managed `byte[]`, then call `qpdfsharp_free_buffer` in a `finally`.
- **Status mapping:**
  - `PASSWORD_REQUIRED` → `QpdfException(PasswordRequired)`.
  - `INVALID_PDF` → `QpdfException(InvalidPdf)`.
  - `INTERNAL_ERROR` → `QpdfException(Internal)`.
  - `INVALID_ARGUMENT` → `ArgumentException`. It should be unreachable; carry the native message.
- **File overload:**
  - Validate that both paths are non-null and non-empty and that the input exists (`FileNotFoundException`).
  - `File.ReadAllBytes` the input, protect it, then write to `outputPath + ".tmp"` and `File.Move(tmp, outputPath, overwrite: true)`. This means a failure never leaves a partial file, and `inputPath == outputPath` works.
- `QpdfVersion`: `Marshal.PtrToStringUTF8` of the native version pointer, computed once lazily.

- [ ] **Step 1: Scaffold the props files, `global.json`, the solution, and `QpdfSharp.csproj`** using the Global Constraints values. Add NuGet metadata:
  - `PackageId`, `Description`, `PackageLicenseExpression MIT`, `RepositoryUrl`, `PackageTags pdf;qpdf;encryption;password`.
  - `PackageReadmeFile` = `PACKAGE-README.md` (as SkiaSharp does).
  - `IncludeSymbols` and `SymbolPackageFormat snupkg`.
  - Pack `External-Dependency-Info.txt` as `THIRD-PARTY-NOTICES.txt`.

- [ ] **Step 2: Implement the types above.**

- [ ] **Step 3: Build**

Run: `dotnet build QpdfSharp.slnx -c Release`
Expected: `Build succeeded. 0 Warning(s) 0 Error(s)` for both TFMs. This task's behaviour is tested in Task 4. It is split out so the API surface can be reviewed on its own.

- [ ] **Step 4: Update `AGENTS.md`** with the `dotnet build` command, then commit.

```bash
git add -A && git commit -m "feat: add managed QpdfSharp API over libQpdfSharp"
```

---

### Task 4: Managed test suite

**Files:**
- Create: `binding/IncludeNativeAssets.QpdfSharp.targets` (the analogue of SkiaSharp's `binding/IncludeNativeAssets.SkiaSharp.targets`)
- Create: `tests/QpdfSharp.Tests/QpdfSharp.Tests.csproj`:
  - `net8.0;net10.0`, `OutputType Exe`.
  - Packages `xunit.v3` and `Microsoft.Testing.Extensions.TrxReport` at current stable versions in `Directory.Packages.props`.
  - Imports `..\..\binding\IncludeNativeAssets.QpdfSharp.targets`.
  - Links `..\Content\**\*` as `Content\…` with `CopyToOutputDirectory`.
- Create: `tests/QpdfSharp.Tests/PdfProtectionTests.cs`, `PasswordValidationTests.cs`, `ConcurrencyTests.cs`, `FileOverloadTests.cs`, `NativeCompatibilityTests.cs`
- Modify: `QpdfSharp.slnx`

**Interfaces:**
- Consumes: Task 3's public API, `InspectEncryption` and `QpdfSharpVersion`; `tests/Content/pdfs/*` from Task 2; `output/native/…` from Task 1/2.
- Produces: `IncludeNativeAssets.QpdfSharp.targets`, which CI (Task 6) reuses.
  - It computes `BuildArch` as SkiaSharp does: `$([System.Runtime.InteropServices.RuntimeInformation]::OSArchitecture.ToString().ToLowerInvariant)`.
  - It adds the first matching file as `Content` with `CopyToOutputDirectory=PreserveNewest` and `Visible=False`. On Windows that is `output/native/windows/$(BuildArch)/libQpdfSharp.dll`; on macOS `output/native/osx/libQpdfSharp.dylib`; on Linux `output/native/linux/$(BuildArch)/libQpdfSharp.so`.
  - If the file is missing, a target with `BeforeTargets="Build"` fails with: `libQpdfSharp for this machine was not found at <path>. Build it first with native/<platform>/build.*`.

- [ ] **Step 1: Write the tests.** All of them use `InspectEncryption` as the oracle.
  - `Protect_ReturnsR6EncryptedPdf`: `info.IsEncrypted`, `info.Revision == 6`.
  - `Protect_OwnerPasswordMatches_AndNoUserPasswordNeeded`: inspect with the owner gives `OwnerPasswordMatched`; inspect with `""` gives `UserPasswordMatched` and `!OwnerPasswordMatched`.
  - `Protect_DefaultGrantsNoPermissions`: `info.Permissions == PdfPermissions.None`.
  - `Protect_PermissionsRoundTrip` (`[Theory]` over every single flag, plus `All`): inspected permissions equal the requested value.
  - `Protect_UndefinedPermissionBits_Throws`: `(PdfPermissions)0x80` → `ArgumentOutOfRangeException`.
  - `Protect_EmptyInput_Throws` (Review Focus 5): `ArgumentException`.
  - `Protect_GarbageInput_ThrowsInvalidPdf` (Review Focus 5): `QpdfException.Kind == InvalidPdf`.
  - `Protect_UserPasswordProtectedInput_ThrowsPasswordRequired` (Review Focus 2).
  - `Protect_AlreadyOwnerProtected_Reprotects` (Review Focus 3): protect with `"old-owner"`, then with `"new-owner"`. `"new-owner"` matches as owner; `"old-owner"` does not.
  - `PasswordValidationTests` (Review Focus 1):
    - `null` → `ArgumentNullException`; `""` → `ArgumentException`; `"a\0b"` → `ArgumentException`.
    - 127 ASCII chars succeed; 128 ASCII chars → `ArgumentException`; 64 × `"é"` (128 UTF-8 bytes) → `ArgumentException`.
    - Protect with `"päss"`; inspect with `"päss"` (U+00E4) gives `OwnerPasswordMatched`.
  - `ConcurrencyTests.Protect_32ParallelCalls_AllSucceed` (Review Focus 4): use `Parallel.ForAsync(0, 32, …)` with distinct passwords `"owner-{i}"`. Each output inspects with its own password as owner and with `"owner-{(i+1)%32}"` as not-owner.
  - `FileOverloadTests`:
    - `ProtectFile_WritesProtectedOutput`.
    - `ProtectFile_SameInputAndOutputPath_Works`.
    - `ProtectFile_MissingInput_ThrowsFileNotFound`.
    - `ProtectFile_InvalidPdf_LeavesNoOutputFile`: neither the output nor `.tmp` exists afterwards.
  - `NativeCompatibilityTests`:
    - `QpdfVersion_Is12_4_2`.
    - `CheckNativeLibraryCompatible_Matching_ReturnsTrue` with `(1, 1, true)`.
    - `CheckNativeLibraryCompatible_Mismatch_Throws` with `(1, 2, true)` → `InvalidOperationException` whose message contains `"ABI 2"`.
    - `CheckNativeLibraryCompatible_Mismatch_NoThrow_ReturnsFalse` with `(1, 2, false)`.

- [ ] **Step 2: Run without the native lib to see the fail-fast message**

Run: `mv output/native output/native.bak; dotnet test --project tests/QpdfSharp.Tests; mv output/native.bak output/native`
Expected: the build fails with the "libQpdfSharp for this machine was not found" error.

- [ ] **Step 3: Run the suite**

Run: `dotnet test --solution QpdfSharp.slnx -c Release`
Expected: all tests pass on both `net8.0` and `net10.0`. If any test fails, fix the library (Task 3 code), not the expectation, unless the expectation contradicts the Design Decisions.

- [ ] **Step 4: Update `AGENTS.md`** with the test command, then commit.

```bash
git add -A && git commit -m "test: add managed test suite for owner-password protection"
```

---

### Task 5: Native-asset NuGet packages and a package-consuming sample

**Files:**
- Create: `binding/QpdfSharp.NativeAssets.Win32/QpdfSharp.NativeAssets.Win32.csproj`, `binding/QpdfSharp.NativeAssets.Linux/QpdfSharp.NativeAssets.Linux.csproj`, `binding/QpdfSharp.NativeAssets.macOS/QpdfSharp.NativeAssets.macOS.csproj`
- Create: `binding/NativeAssets.Build.targets`, `binding/Directory.Build.targets` (imports the root `Directory.Build.targets` if one exists, plus `NativeAssets.Build.targets` when `$(MSBuildProjectName.Contains('.NativeAssets.'))`)
- Modify: `binding/QpdfSharp/QpdfSharp.csproj` (plain `ProjectReference` to each native-asset project, as `SkiaSharp.csproj` does), `QpdfSharp.slnx`
- Create: `samples/Basic/Console/QpdfSharpSample/QpdfSharpSample.csproj` (`net8.0`, `PackageReference QpdfSharp` at `$(QpdfSharpPackageVersion)`), `samples/Basic/Console/QpdfSharpSample/Program.cs`, `samples/Basic/Console/nuget.config`. The sample is **not** in `QpdfSharp.slnx`.
- Create: `scripts/pack.sh`, `scripts/run-sample.sh`

**Interfaces:**
- Consumes: `output/native/…` (Design Decision 6 table); Task 3's project.
- Produces:
  - Packages: `output/nuget/QpdfSharp.<ver>.nupkg`, `QpdfSharp.NativeAssets.{Win32,Linux,macOS}.<ver>.nupkg`.
  - Package contents: the paths in the Design Decision 6 table, plus `lib/net8.0/_._`, `lib/net10.0/_._` and `THIRD-PARTY-NOTICES.txt`.
  - Native-asset project items use SkiaSharp's pattern: `<NativeAssetPackageFile Include="..\..\output\native\windows\x64\libQpdfSharp.dll" RuntimeIdentifier="win-x64" />`.
  - `NativeAssets.Build.targets` handles all of the following:
    - `IncludeBuildOutput=false` and no symbols.
    - The `_._` placeholder per TFM (SkiaSharp's `_IncludePlaceholders` target).
    - Mapping `NativeAssetPackageFile` to `runtimes/%(RuntimeIdentifier)/native/`.
    - Packing `External-Dependency-Info.txt` as `THIRD-PARTY-NOTICES.txt`.
    - A pack-only missing-file check (`BeforeTargets="GenerateNuspec"`) that names each missing file. It must **not** run on `dotnet build`/`dotnet test`. Setting `-p:AllowMissingNativeAssets=true` downgrades it to a warning.
  - `scripts/pack.sh [--allow-missing-native] [extra dotnet pack args…]`: runs `dotnet pack QpdfSharp.slnx -c Release -o output/nuget`. The flag maps to `AllowMissingNativeAssets=true`. Extra arguments (for example `-p:Version=…`) pass through to `dotnet pack`.
  - `scripts/run-sample.sh <version>`:
    1. Restores the sample with `--configfile samples/Basic/Console/nuget.config` (local source `../../../output/nuget` plus nuget.org) and `--packages output/sample-packages`. The separate package folder stops a stale global-cache copy of the same version being used.
    2. Runs the sample with `tests/Content/pdfs/minimal.pdf`, `output/sample/protected.pdf` and `sample-owner`.
    3. Asserts exit code 0 and that the output exists.
  - Sample CLI: `QpdfSharpSample <input.pdf> <output.pdf> <owner-password>`. Prints `qpdf <version>` and `Protected <output>`, and exits 0. On `QpdfException` it prints `Error (<Kind>): <message>` and exits 2.

- [ ] **Step 1: Implement the projects, the targets files and the scripts.**

- [ ] **Step 2: Pack locally (macOS binary only)**

Run: `./scripts/pack.sh --allow-missing-native`
Expected: four `.nupkg` files (plus `QpdfSharp.<ver>.snupkg`) in `output/nuget/`, with warnings naming the four missing Windows/Linux binaries.

- [ ] **Step 3: Check package contents**

Run: `unzip -l output/nuget/QpdfSharp.NativeAssets.macOS.0.1.0.nupkg && unzip -p output/nuget/QpdfSharp.0.1.0.nupkg QpdfSharp.nuspec`
Expected: the macOS package lists `runtimes/osx/native/libQpdfSharp.dylib`, `lib/net8.0/_._`, `lib/net10.0/_._` and `THIRD-PARTY-NOTICES.txt`. The nuspec lists the three `QpdfSharp.NativeAssets.*` dependencies at `0.1.0` for both `net8.0` and `net10.0`.

- [ ] **Step 4: Check that strict pack fails fast**

Run: `./scripts/pack.sh`
Expected: non-zero exit, and an error naming the four missing binaries.

- [ ] **Step 5: Run the sample against the packages**

Run: `./scripts/run-sample.sh 0.1.0 && qpdf --show-encryption --password=sample-owner output/sample/protected.pdf`
Expected: the sample prints `Protected …`. qpdf prints `R = 6` and `Supplied password is owner password`.

- [ ] **Step 6: Check that tests are unaffected:** `dotnet test --solution QpdfSharp.slnx -c Release` still passes.

- [ ] **Step 7: Update `AGENTS.md`** with the pack and sample commands, then commit.

```bash
git add -A && git commit -m "build: add native-asset NuGet packages and package smoke sample"
```

---

### Task 6: CI pipeline — native matrix, tests, pack, package smoke

**Files:**
- Create: `native/linux/build.sh`, `native/linux/check.sh` (Linux counterparts of the macOS scripts)
- Create: `native/windows/build.ps1`, `native/windows/check.ps1`
- Create: `native/libQpdfSharp/triplets/qpdfsharp-linux-x64.cmake`, `qpdfsharp-linux-arm64.cmake`, `qpdfsharp-windows-x64.cmake`, `qpdfsharp-windows-arm64.cmake`
- Create: `.github/workflows/ci.yml`

**Interfaces:**
- Consumes: every script and targets file from Tasks 1–5.
- Produces:
  - `native/linux/build.sh <x64|arm64>` → `output/native/linux/<arch>/libQpdfSharp.so`, running CTest.
  - `native/linux/check.sh <x64|arm64>`:
    - Exports: `nm -D --defined-only` lists only the five functions. Ignore version-node symbols.
    - Dependencies: `readelf -d` `NEEDED` entries are in the allow-list.
    - glibc: `objdump -T | grep -o 'GLIBC_[0-9.]*' | sort -V | tail -1` ≤ 2.28.
  - `native/windows/build.ps1 -Arch <x64|arm64>` → `output/native/windows/<arch>/libQpdfSharp.dll`, running CTest. It expects an MSVC developer environment.
  - `native/windows/check.ps1 -Arch <x64|arm64>`: `dumpbin /exports` lists exactly the five functions, and `dumpbin /dependents` has no `MSVCP*`, `VCRUNTIME*`, `api-ms-win-crt-*`, `zlib*`, `jpeg*` or `qpdf*` entries.
  - Triplets: Linux uses `VCPKG_CMAKE_SYSTEM_NAME Linux` and `VCPKG_CRT_LINKAGE dynamic`. Windows uses `VCPKG_CRT_LINKAGE static`. All use `VCPKG_LIBRARY_LINKAGE static` and `VCPKG_BUILD_TYPE release`.
  - Workflow `ci.yml`, triggered by `push` to `main` and `pull_request`. The package version is `0.1.0-ci.${{ github.run_number }}`. Job names, which branch protection relies on: `native`, `test`, `pack`, `package-smoke`.

**Job design** (same stage order as SkiaSharp's pipeline: native → package → tests):
- **`native`**: matrix of five entries.

  | entry | runner | container | build / check |
  |---|---|---|---|
  | `linux-x64` | `ubuntu-24.04` | `quay.io/pypa/manylinux_2_28_x86_64` | `native/linux/build.sh x64` / `check.sh x64` |
  | `linux-arm64` | `ubuntu-24.04-arm` | `quay.io/pypa/manylinux_2_28_aarch64` | `native/linux/build.sh arm64` / `check.sh arm64` |
  | `osx` | `macos-15` | — | `native/macos/build.sh` / `check.sh` (universal) |
  | `windows-x64` | `windows-2025` | — (MSVC env via `ilammy/msvc-dev-cmd@v1`, `arch: x64`) | `build.ps1 -Arch x64` / `check.ps1 -Arch x64` |
  | `windows-arm64` | `windows-11-arm` | — (`arch: arm64`) | `build.ps1 -Arch arm64` / `check.ps1 -Arch arm64` |

  Steps:
  1. Checkout with `submodules: recursive`.
  2. Clone vcpkg at `2026.10.16` and bootstrap it.
  3. `actions/cache` keyed on `hashFiles('native/libQpdfSharp/vcpkg.json','native/libQpdfSharp/triplets/**')` + entry, for `VCPKG_DEFAULT_BINARY_CACHE`.
  4. In the manylinux containers, `dnf install` `ninja-build`, `zip`, `unzip`, `nasm`, `pkgconf-pkg-config` and `perl-IPC-Cmd`. Use the image's `gcc-toolset` and a CMake ≥ 3.25 (`pipx install cmake` if needed).
  5. Run build and check.
  6. Upload artifact `native-<entry>` containing that entry's `output/native/...` subtree, with paths preserved.
- **`test`** (`needs: native`): six runners, **without** containers, to prove the manylinux-built binary runs on a normal distro.

  | runner | artifact |
  |---|---|
  | `ubuntu-24.04` | `native-linux-x64` |
  | `ubuntu-24.04-arm` | `native-linux-arm64` |
  | `macos-15` (arm64) | `native-osx` |
  | `macos-15-intel` | `native-osx` (exercises the x86_64 slice) |
  | `windows-2025` | `native-windows-x64` |
  | `windows-11-arm` | `native-windows-arm64` |

  Steps:
  1. `actions/setup-dotnet` with `8.0.x` and `10.0.x`.
  2. Download the artifact into `output/native/`.
  3. Run `dotnet test --solution QpdfSharp.slnx -c Release -- --report-trx`.
  4. Upload the TRX files on failure.
- **`pack`** (`needs: native`):
  1. Download all five native artifacts into `output/native/`.
  2. Run `./scripts/pack.sh -p:Version=<version> -p:ContinuousIntegrationBuild=true` (strict mode).
  3. Upload `nuget-packages`.
- **`package-smoke`** (`needs: [pack, test]`): the same six runners, plus one extra entry `linux-x64-ubi8` on `ubuntu-24.04` with `container: registry.access.redhat.com/ubi8/dotnet-80` (glibc 2.28; `options: --user root`).
  1. Download `nuget-packages` into `output/nuget`.
  2. Run `scripts/run-sample.sh <version>`. On Windows run it under `bash` (Git Bash is preinstalled on GitHub runners).
  3. On the `ubuntu-24.04` entry only, also run `sudo apt-get install -y qpdf`, then assert `qpdf --show-encryption --password=sample-owner output/sample/protected.pdf` contains `Supplied password is owner password` and `R = 6`. This is an oracle independent of our inspect code.
- Workflow-level: `permissions: contents: read`, `concurrency: ci-${{ github.ref }}` with `cancel-in-progress: true` for PRs. Pin third-party actions to a major version tag.

- [ ] **Step 1: Write the Linux and Windows build/check scripts and triplets.** Mirror the macOS scripts' structure and messages. If Docker is available locally, run `native/linux/build.sh` inside `quay.io/pypa/manylinux_2_28_aarch64` on the Apple-silicon machine before pushing.

- [ ] **Step 2: Write `.github/workflows/ci.yml`.**

- [ ] **Step 3: Push a branch and open a PR**

```bash
git checkout -b ci/pipeline && git add -A && git commit -m "ci: add native matrix, test, pack and package smoke workflow" && git push -u origin ci/pipeline
```

- [ ] **Step 4: Iterate until green**

Expected: all `native` (5), `test` (6), `pack` (1) and `package-smoke` (7) jobs pass. The `native` logs for `linux-*` show `check.sh` reporting max `GLIBC_2.28` or lower. The `windows-*` logs show no CRT dependents. Fix the scripts or CMake for platform-specific failures; never loosen the Global Constraints allow-lists to get green. If a constraint is truly impossible on a platform, stop and report it.

- [ ] **Step 5: Prove the gates bite.** On the branch, temporarily add a sixth exported function to the shim (and to the export lists) without updating the `check` scripts, and push. `native` must fail on every entry at the check step. Revert, and push again to get back to green.

---

### Task 7: Documentation

**Files:**
- Modify: `README.md`, `AGENTS.md`
- Create: `docs/building.md`

**Interfaces:**
- Consumes: the scripts and public API from Tasks 1–6; the `nuget-packages` CI artifact.
- Produces: user-facing and contributor-facing docs. Nothing later depends on them.

**Behaviour:**
- README:
  - What the library does.
  - Install: not on nuget.org yet. Either download the `nuget-packages` artifact from a green CI run and add it as a local NuGet source, or build from source (link to `docs/building.md`).
  - A 6-line usage example of both overloads.
  - A table of `PdfPermissions` flags.
  - The supported-platform table (Design Decision 6) with the glibc ≥ 2.28 / macOS ≥ 12 / Windows 10+ minimums.
  - The note that accessibility is always allowed (Design Decision 4).
  - The note on password normalisation and the 127-byte limit.
  - The repository layout, with a one-line pointer to SkiaSharp as the structural model.
  - Third-party licenses.
- `docs/building.md`:
  - Prerequisites: the Local prerequisites section above.
  - The build flow: `native/macos/build.sh` → `dotnet test` → `scripts/pack.sh --allow-missing-native` → `scripts/run-sample.sh`.
  - How to bump the qpdf submodule and the vcpkg baseline together. Remind the reader that `QpdfVersion_Is12_4_2` and the smoke test's version assertion must change in the same commit, and that `QPDFSHARP_ABI_VERSION` and `QpdfSharpVersion.SupportedAbiVersion` must be bumped together whenever the C header changes incompatibly.
- `AGENTS.md`: make sure every command is current and links to `docs/building.md`.

- [ ] **Step 1: Write the README, `docs/building.md` and the `AGENTS.md` updates.**

- [ ] **Step 2: Follow `docs/building.md` in a clean clone**

Run: `git clone --recursive https://github.com/MitchellCash/qpdfsharp <scratch-dir>/qpdfsharp-docs-check`, then run every command in `docs/building.md` in order, exactly as written.
Expected: every command succeeds and the sample prints `Protected …`. If a command needs an unstated step, fix the doc.

- [ ] **Step 3: Check the README example compiles.** Paste the usage example into the sample's `Program.cs` on a throwaway branch and run `./scripts/run-sample.sh 0.1.0`. Expected: it builds and runs. Discard the branch.

- [ ] **Step 4: Commit and open a PR**

```bash
git add -A && git commit -m "docs: add README usage and build-from-source guide"
```

---

## Out of scope (candidate follow-ups)

- Publishing to nuget.org (a tag-driven release workflow with trusted publishing and a GitHub Release).
- Cake-based build orchestration and an Azure Pipelines mirror of SkiaSharp's CI.
- linux-musl (Alpine) and linux-bionic RIDs, plus a `NativeAssets.Linux.NoDependencies` variant.
- Mobile, Mac Catalyst and WASM RIDs.
- .NET Framework / `netstandard` targets (these would bring in SkiaSharp's `LibraryLoader`, `USE_DELEGATES` path and `buildTransitive/net4` targets).
- User passwords.
- Decrypting files.
- `Stream` overloads.
- Exposing `InspectEncryption` publicly.
- Native AOT publish smoke test.
- Signing native binaries.
