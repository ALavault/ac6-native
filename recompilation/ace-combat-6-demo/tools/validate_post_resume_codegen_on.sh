#!/usr/bin/env bash
set -euo pipefail

export TMPDIR=/fastdata/lavaulta/tmp

if (( $# != 0 )); then
  printf 'usage: %s\n' "${BASH_SOURCE[0]}" >&2
  exit 2
fi

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
PORTFOLIO_ROOT="$(cd -- "$SCRIPT_DIR/../../.." && pwd -P)"
PROJECT="$PORTFOLIO_ROOT/recompilation/ace-combat-6-demo"
BUILD="$PROJECT/build-codegen-on"
XEX="$PORTFOLIO_ROOT/demo-game-file/extracted/stfs-root/Default.xex"
EXPECTED_XEX_SHA256="de917873f601e2a2208d75ab907e918ce941a42378d0d088705ecb4477405da8"
EXPECTED_BASEFILE_SHA256="b98a9ac1f5a2da4c0b6e3bbae1d6cf7fe8c1fc2292b1cef51cc627581aa14218"
EXPECTED_XENONRECOMP_COMMIT="ddd128bcca99fe8bfbb99bea583c972351fa6ace"
EXPECTED_LIBMSPACK_COMMIT="305907723a4e7ab2018e58040059ffb5e77db837"

case "$XEX" in
  *retail*|*Retail*)
    printf 'retail XEX path refused: %s\n' "$XEX" >&2
    exit 2
    ;;
esac
test -r "$XEX"
actual_xex_sha256="$(sha256sum -- "$XEX" | sed 's/[[:space:]].*$//')"
test "$actual_xex_sha256" = "$EXPECTED_XEX_SHA256"
test -f "$PORTFOLIO_ROOT/analysis/demo/ac6-demo-ghidra-manifest.json"
test -f "$BUILD/CMakeCache.txt"
grep -qx 'AC6_DEMO_ENABLE_CODEGEN:BOOL=ON' "$BUILD/CMakeCache.txt"

# Qualify the pinned toolchain before any build. build_demo.py intentionally
# applies these two strict patches, so that exact patch footprint is the only
# accepted dirty state; all other checkout/submodule changes fail closed.
CODEGEN_CHECKOUT="$BUILD/codegen/XenonRecomp"
LIBMSPACK_CHECKOUT="$CODEGEN_CHECKOUT/thirdparty/libmspack"
LZX_SOURCE="$LIBMSPACK_CHECKOUT/libmspack/mspack/lzxd.c"
test -d "$CODEGEN_CHECKOUT"
test -d "$LIBMSPACK_CHECKOUT" || test -f "$LIBMSPACK_CHECKOUT/.git"
test "$(git -C "$CODEGEN_CHECKOUT" rev-parse HEAD)" = \
  "$EXPECTED_XENONRECOMP_COMMIT"
test "$(git -C "$CODEGEN_CHECKOUT" ls-tree HEAD thirdparty/libmspack)" = \
  "160000 commit ${EXPECTED_LIBMSPACK_COMMIT}"$'\tthirdparty/libmspack'
test "$(git -C "$LIBMSPACK_CHECKOUT" rev-parse HEAD)" = \
  "$EXPECTED_LIBMSPACK_COMMIT"
test -f "$LZX_SOURCE"

root_status="$(git -C "$CODEGEN_CHECKOUT" status --short --untracked-files=all)"
expected_patched_status=$' M XenonRecomp/recompiler.cpp\n M XenonRecomp/recompiler_config.cpp\n M XenonRecomp/recompiler_config.h'
if [[ -z "$root_status" ]]; then
  git -C "$CODEGEN_CHECKOUT" apply --check \
    "$PROJECT/patches/xenonrecomp-strict-recompiler.patch"
  git -C "$CODEGEN_CHECKOUT" apply --check \
    "$PROJECT/patches/xenonrecomp-strict-config.patch"
elif [[ "$root_status" == "$expected_patched_status" ]]; then
  git -C "$CODEGEN_CHECKOUT" apply --reverse --check \
    "$PROJECT/patches/xenonrecomp-strict-recompiler.patch"
  git -C "$CODEGEN_CHECKOUT" apply --reverse --check \
    "$PROJECT/patches/xenonrecomp-strict-config.patch"
else
  printf 'unexpected XenonRecomp checkout status:\n%s\n' "$root_status" >&2
  exit 2
fi
submodule_status="$(git -C "$LIBMSPACK_CHECKOUT" status \
  --short --untracked-files=all)"
if [[ -n "$submodule_status" ]]; then
  printf 'non-clean libmspack submodule status:\n%s\n' \
    "$submodule_status" >&2
  exit 2
fi

# Regenerate through the tracked custom-command dependencies, then compile the
# complete codegen-on tree without a clean that can delete tracked sources.
cmake --build "$BUILD" --target ac6-demo-codegen -j12
cmake --build "$BUILD" -j16

BASEFILE="$BUILD/codegen/xex-basefile.bin"
actual_basefile_sha256="$(sha256sum -- "$BASEFILE" | sed 's/[[:space:]].*$//')"
test "$actual_basefile_sha256" = "$EXPECTED_BASEFILE_SHA256"

ADAPTER="$PROJECT/tools/ppc_context_adapter.h"
GENERATED_ADAPTER="$BUILD/codegen/ppc_context_adapter.h"
cmp "$GENERATED_ADAPTER" "$ADAPTER"
rg -q 'AC6_PPC_RECORD_POST_RESUME_VECTOR_READ' "$GENERATED_ADAPTER"
rg -q 'AC6_PPC_POST_RESUME_VECTOR_FAST_ENABLED' "$GENERATED_ADAPTER"
rg -q 'AC6_PPC_FUNCTION_ENTRY_CONTEXT|AC6_PPC_SET_LOAD_SITE' "$GENERATED_ADAPTER"

SDL_AUDIODRIVER=dummy xvfb-run -a ctest --test-dir "$BUILD" --output-on-failure

# This focused suite includes the controlled mapper success/ambiguity/XEX
# fixtures; it does not claim those fixtures are PAL runtime evidence.
python3 -m unittest discover -s "$PROJECT/tests" -p 'test_post_resume_probe.py'
python3 -m py_compile "$PROJECT/tools/map_generated_guest_load_sites.py"
python3 "$PORTFOLIO_ROOT/tools/audit_cpp_complexity.py" \
  --root "$PROJECT" \
  --baseline "$PROJECT/config/source-complexity-baseline.json" --check
python3 "$PROJECT/tools/audit_demo_sources.py"
git -C "$PORTFOLIO_ROOT" diff --check
