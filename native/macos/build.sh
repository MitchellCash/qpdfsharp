#!/usr/bin/env bash
#
# Builds the universal (arm64 + x86_64) macOS libQpdfSharp into output/native/osx/libQpdfSharp.dylib.
#
# Each architecture is configured and built separately in build/native/osx-<arch>, with qpdf, zlib
# and libjpeg-turbo linked statically, and its CTest tests are run when this machine can execute
# that slice. The slices are then merged with lipo.
#
# Requires: VCPKG_ROOT pointing at a bootstrapped vcpkg checkout, cmake, ninja, pkg-config, nasm.

set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
SOURCE_DIR="$REPO_ROOT/native/libQpdfSharp"
OUTPUT_DIR="$REPO_ROOT/output/native/osx"
LIBRARY_NAME="libQpdfSharp.dylib"

# "<CMAKE_OSX_ARCHITECTURES value>:<vcpkg triplet arch>"
SLICES=("arm64:arm64" "x86_64:x64")

if [[ -z "${VCPKG_ROOT:-}" ]]; then
  echo "error: VCPKG_ROOT is not set. Point it at a bootstrapped vcpkg checkout." >&2
  exit 1
fi
VCPKG_TOOLCHAIN="$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake"
if [[ ! -f "$VCPKG_TOOLCHAIN" ]]; then
  echo "error: vcpkg toolchain not found at $VCPKG_TOOLCHAIN" >&2
  exit 1
fi

can_execute_slice() {
  local arch="$1"
  arch "-$arch" /usr/bin/true 2>/dev/null
}

slice_libraries=()
for slice in "${SLICES[@]}"; do
  arch="${slice%%:*}"
  triplet_arch="${slice##*:}"
  build_dir="$REPO_ROOT/build/native/osx-$arch"

  echo "==> Configuring $arch in $build_dir"
  cmake -S "$SOURCE_DIR" -B "$build_dir" -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_TOOLCHAIN_FILE="$VCPKG_TOOLCHAIN" \
    -DVCPKG_TARGET_TRIPLET="qpdfsharp-osx-$triplet_arch" \
    -DVCPKG_OVERLAY_TRIPLETS="$SOURCE_DIR/triplets" \
    -DCMAKE_OSX_ARCHITECTURES="$arch"

  echo "==> Building $arch"
  cmake --build "$build_dir"

  if can_execute_slice "$arch"; then
    echo "==> Testing $arch"
    ctest --test-dir "$build_dir" --output-on-failure
  else
    echo "==> Skipping $arch tests: this machine cannot execute $arch binaries (is Rosetta installed?)"
  fi

  slice_libraries+=("$build_dir/$LIBRARY_NAME")
done

mkdir -p "$OUTPUT_DIR"
lipo -create "${slice_libraries[@]}" -output "$OUTPUT_DIR/$LIBRARY_NAME"
echo "==> Wrote $OUTPUT_DIR/$LIBRARY_NAME ($(lipo -archs "$OUTPUT_DIR/$LIBRARY_NAME"))"
