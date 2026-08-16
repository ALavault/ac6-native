#!/usr/bin/env bash
set -euo pipefail

: "${TMPDIR:?TMPDIR must be set}"

if [[ $# -ne 3 ]]; then
  echo "usage: $0 REXGLUE_SDK_ROOT SIMDE_ROOT OUTPUT" >&2
  exit 2
fi

sdk_root=$(realpath "$1")
simde_root=$(realpath "$2")
output=$(realpath -m "$3")
tmp_root=$(realpath "$TMPDIR")
case "$output" in
  "$tmp_root"/*) ;;
  *) echo "output must stay under TMPDIR" >&2; exit 2 ;;
esac

source_root=$(cd "$(dirname "$0")" && pwd)
library_root="$sdk_root/out/linux-amd64"
git_root=$(git -C "$sdk_root" rev-parse --show-toplevel)
sdk_relative=$(realpath --relative-to="$git_root" "$sdk_root")
if [[ $(git -C "$git_root" rev-parse HEAD) != \
      dcd41b7457fcac8242f8ef40de83d1719390d5af ]] ||
   [[ $(git -C "$git_root" rev-parse "HEAD:$sdk_relative") != \
      741541d6035616dc406f7d74c2fe8f155913c77b ]]; then
  echo "ReXGlue checkout identity mismatch" >&2
  exit 3
fi
for required in \
  "$sdk_root/include/rex/graphics/pipeline/shader/spirv_translator.h" \
  "$sdk_root/thirdparty/renderdoc/renderdoc_app.h" \
  "$library_root/librexgraphics.a" \
  "$simde_root/simde/x86/avx.h"; do
  if [[ ! -f "$required" ]]; then
    echo "missing pinned ReXGlue input: $required" >&2
    exit 3
  fi
done
if [[ $(sha256sum "$library_root/librexgraphics.a" | cut -d' ' -f1) != \
      564a728c0f83217d0e6f1c5c4cb8d829cb9d3c6e04c63e4285b1a449a98243a5 ]]; then
  echo "ReXGlue graphics archive identity mismatch" >&2
  exit 3
fi

compiler=${CXX:-clang++}
if ! "$compiler" --version | head -1 | grep -q 'clang'; then
  echo "ReXGlue oracle CLI requires Clang" >&2
  exit 3
fi
compiler_command=("$compiler")
if command -v ccache >/dev/null 2>&1; then
  compiler_command=(ccache "$compiler")
fi

"${compiler_command[@]}" -std=c++23 \
  -I"$sdk_root/include" \
  -I"$sdk_root/thirdparty/renderdoc" \
  -I"$sdk_root/thirdparty/spirv-headers/include" \
  -I"$sdk_root/thirdparty/vulkan-headers/include" \
  -I"$sdk_root/thirdparty/fmt/include" \
  -I"$sdk_root/thirdparty/spdlog/include" \
  -I"$simde_root" \
  -I"$sdk_root/thirdparty/glslang" \
  -I"$sdk_root/thirdparty/glslang/SPIRV" \
  -I"$sdk_root/thirdparty/volk" \
  -I"$sdk_root/thirdparty/vulkan-memory-allocator/include" \
  "$source_root/rexglue_shader_cli.cpp" \
  -L"$library_root" -Wl,--start-group \
  -lrexgraphics -lrexcore -lrexsystem -lrexui \
  -lSPIRV -lglslang -lMachineIndependent -lGenericCodeGen \
  -lOSDependent -lOGLCompiler -lfmt -lxxhash -lsnappy -lvolk -lspdlog \
  -Wl,--end-group -lcrypto -lpthread -ldl -o "$output"
