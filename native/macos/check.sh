#!/usr/bin/env bash
#
# Verifies a macOS libQpdfSharp.dylib against the project's binary constraints:
#   - exactly the expected qpdfsharp_* symbols are exported (nothing from qpdf, zlib or libjpeg),
#   - it depends only on the system C++ runtime and libSystem,
#   - every slice targets macOS 12.0 or earlier,
#   - it is universal (x86_64 and arm64).
#
# Usage: native/macos/check.sh [path]   (default: output/native/osx/libQpdfSharp.dylib)
# Prints every violation and exits 1, or prints OK.

set -euo pipefail

# Keep in sync with native/macos/libQpdfSharp/libQpdfSharp.exports.
EXPECTED_EXPORTS=(
  _qpdfsharp_get_qpdf_version
  _qpdfsharp_get_abi_version
  _qpdfsharp_protect_with_owner_password
  _qpdfsharp_inspect_encryption
  _qpdfsharp_free_buffer
)
ALLOWED_DEPENDENCIES=(
  @rpath/libQpdfSharp.dylib
  /usr/lib/libc++.1.dylib
  /usr/lib/libSystem.B.dylib
)
REQUIRED_ARCHS=(x86_64 arm64)
MAX_MINOS="12.0"

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
LIBRARY="${1:-$REPO_ROOT/output/native/osx/libQpdfSharp.dylib}"

if [[ ! -f "$LIBRARY" ]]; then
  echo "error: $LIBRARY does not exist. Build it with native/macos/build.sh." >&2
  exit 1
fi

THIN_DIR="$(mktemp -d)"
trap 'rm -rf "$THIN_DIR"' EXIT

violations=()

is_in_list() {
  local needle="$1"
  shift
  local item
  for item in "$@"; do
    [[ "$item" == "$needle" ]] && return 0
  done
  return 1
}

# Succeeds when version $1 <= version $2 (both "major.minor[.patch]").
version_at_most() {
  awk -v a="$1" -v b="$2" 'BEGIN {
    na = split(a, x, "."); nb = split(b, y, ".")
    for (i = 1; i <= 3; i++) {
      xi = (i <= na) ? x[i] + 0 : 0; yi = (i <= nb) ? y[i] + 0 : 0
      if (xi < yi) exit 0
      if (xi > yi) exit 1
    }
    exit 0
  }'
}

# --- architectures ---
read -r -a archs <<< "$(lipo -archs "$LIBRARY")"
for required in "${REQUIRED_ARCHS[@]}"; do
  if ! is_in_list "$required" "${archs[@]}"; then
    violations+=("architectures: $required slice missing (found: ${archs[*]})")
  fi
done

for arch in "${archs[@]}"; do
  slice="$THIN_DIR/libQpdfSharp-$arch.dylib"
  if [[ ${#archs[@]} -gt 1 ]]; then
    lipo "$LIBRARY" -thin "$arch" -output "$slice"
  else
    cp "$LIBRARY" "$slice"
  fi

  # --- exports ---
  actual_exports=()
  while IFS= read -r symbol; do
    actual_exports+=("$symbol")
  done < <(nm -gUj "$slice")
  for symbol in ${actual_exports[@]+"${actual_exports[@]}"}; do
    if ! is_in_list "$symbol" "${EXPECTED_EXPORTS[@]}"; then
      violations+=("$arch exports: unexpected symbol $symbol")
    fi
  done
  for symbol in "${EXPECTED_EXPORTS[@]}"; do
    if ! is_in_list "$symbol" ${actual_exports[@]+"${actual_exports[@]}"}; then
      violations+=("$arch exports: missing symbol $symbol")
    fi
  done

  # --- dependencies (the first line of otool -L is the file name) ---
  while IFS= read -r dependency; do
    if ! is_in_list "$dependency" "${ALLOWED_DEPENDENCIES[@]}"; then
      violations+=("$arch dependencies: $dependency is not allowed")
    fi
  done < <(otool -L "$slice" | tail -n +2 | awk '{ print $1 }')

  # --- deployment target ---
  minos="$(otool -l "$slice" | awk '$1 == "minos" { print $2; exit }')"
  if [[ -z "$minos" ]]; then
    violations+=("$arch deployment target: no LC_BUILD_VERSION minos found")
  elif ! version_at_most "$minos" "$MAX_MINOS"; then
    violations+=("$arch deployment target: minos $minos is newer than $MAX_MINOS")
  fi
done

if [[ ${#violations[@]} -gt 0 ]]; then
  echo "$LIBRARY has ${#violations[@]} violation(s):" >&2
  printf '  %s\n' "${violations[@]}" >&2
  exit 1
fi

echo "OK"
