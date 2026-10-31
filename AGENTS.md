# AGENTS.md

QpdfSharp is a .NET library that protects PDFs with an owner password through a statically linked
native build of [qpdf](https://github.com/qpdf/qpdf). The repository is laid out like
[SkiaSharp](https://github.com/mono/SkiaSharp): upstream qpdf is a submodule, a small C ABI shim
(`libQpdfSharp`) wraps it, and the managed library binds to that shim.

The implementation plan, with the fixed design decisions and constraints, is
`docs/superpowers/plans/2026-10-31-qpdfsharp-owner-password.md`.

## Layout

Directories marked *(planned)* do not exist yet.

```
qpdfsharp/
├─ AGENTS.md                         # this file
├─ External-Dependency-Info.txt      # third-party licenses; packed as THIRD-PARTY-NOTICES.txt
├─ LICENSE.md  README.md  global.json  Directory.Build.props  Directory.Packages.props  QpdfSharp.slnx
├─ externals/qpdf/                   # submodule @ v12.4.2
├─ native/
│  ├─ libQpdfSharp/                  # shared C ABI shim + CMake project
│  │  ├─ CMakeLists.txt  vcpkg.json  triplets/
│  │  ├─ include/qpdfsharp.h  src/qpdfsharp.cpp  tests/
│  ├─ linux/   build.sh  check.sh  libQpdfSharp/libQpdfSharp.map
│  ├─ macos/   build.sh  check.sh  libQpdfSharp/libQpdfSharp.exports
│  └─ windows/ build.ps1 check.ps1   # (planned)
├─ binding/                          # (planned) managed library and native-asset packages
│  ├─ Directory.Build.targets
│  ├─ NativeAssets.Build.targets
│  ├─ IncludeNativeAssets.QpdfSharp.targets
│  ├─ QpdfSharp/
│  ├─ QpdfSharp.NativeAssets.Linux/
│  ├─ QpdfSharp.NativeAssets.macOS/
│  └─ QpdfSharp.NativeAssets.Win32/
├─ tests/                            # (planned)
│  ├─ Content/pdfs/                  # shared test assets (native and managed tests both use these)
│  └─ QpdfSharp.Tests/
├─ samples/Basic/Console/QpdfSharpSample/   # (planned) consumes the packed .nupkg files
├─ scripts/  pack.sh  run-sample.sh  # (planned)
├─ .github/workflows/ci.yml          # (planned)
└─ output/  build/                   # git-ignored
   └─ output/native/{windows/<arch>, linux/<arch>, osx}/libQpdfSharp.*
```

Only `native/libQpdfSharp/`, the Linux version script and the macOS scripts exist so far; the
Linux `build.sh`/`check.sh` are planned.

## Local prerequisites (macOS)

```bash
brew install cmake ninja pkg-config nasm qpdf
git clone https://github.com/microsoft/vcpkg ~/vcpkg && git -C ~/vcpkg checkout 2026.10.16 && ~/vcpkg/bootstrap-vcpkg.sh -disableMetrics
export VCPKG_ROOT=~/vcpkg
```

After cloning, fetch the qpdf submodule: `git submodule update --init`.

The `qpdf` CLI is only used to generate test assets and as an independent oracle in CI. The library
never shells out to it.

## Build and verify the native library

```bash
./native/macos/build.sh && ./native/macos/check.sh
```

- `build.sh` builds `arm64` and `x86_64` slices in `build/native/osx-<arch>/` (vcpkg installs
  static zlib and libjpeg-turbo per slice), runs CTest for each slice this machine can execute (the
  x86_64 slice runs under Rosetta), and writes the universal
  `output/native/osx/libQpdfSharp.dylib`. It fails if `VCPKG_ROOT` is unset.
- `check.sh [path]` verifies each slice: exactly the expected `qpdfsharp_*` exports, only
  `libc++`/`libSystem` dependencies, `minos` ≤ 12.0, and both architectures present. It prints
  `OK` or every violation.

## Rules that are easy to break

- `libQpdfSharp` exports only `qpdfsharp_*` functions. When adding one, update the header,
  `native/macos/libQpdfSharp/libQpdfSharp.exports` and `EXPECTED_EXPORTS` in
  `native/macos/check.sh` together.
- Bump `QPDFSHARP_ABI_VERSION` in `qpdfsharp.h` whenever the C header changes incompatibly.
- No exception may cross the C boundary.
- Never loosen a `check.sh` allow-list to make a build pass.
