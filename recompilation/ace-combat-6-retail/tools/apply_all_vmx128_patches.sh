#!/bin/bash
# Apply all VMX128 instruction patches to a XenonRecomp codegen output.
# Usage: apply_all_vmx128_patches.sh <codegen-generated-dir>
#
# Patches:
#   vupkd3d128 type 20 (float16->float32)   — ppc_recomp.10.cpp
#   vcmpbfp. (vector bounds + CR6)           — ppc_recomp.11.cpp
#   vpkd3d128 type 3 (float32->float16)      — ppc_recomp.25.cpp
#   blrl (indirect call via r3)              — ppc_recomp.66.cpp

set -euo pipefail

CODEGEN="${1:?usage: $0 <codegen-generated-dir>}"
TOOLS="$(cd "$(dirname "$0")" && pwd)"

echo "Patching $CODEGEN"

python3 "$TOOLS/apply_vupkd3d128_type20_fix.py" "$CODEGEN/ppc_recomp.10.cpp"
python3 "$TOOLS/apply_vcmpbfp_fix.py" "$CODEGEN/ppc_recomp.11.cpp"
python3 "$TOOLS/apply_vpkd3d128_type3_fix.py" "$CODEGEN/ppc_recomp.25.cpp"
python3 "$TOOLS/apply_blrl_indirect_fix.py" "$CODEGEN/ppc_recomp.66.cpp"

remaining=$(grep -c '__builtin_debugtrap' "$CODEGEN"/ppc_recomp.{10,11,25,66}.cpp 2>/dev/null | awk -F: '{s+=$2} END{print s+0}' || true)
echo "remaining debugtraps in patched files: $remaining"
[ "$remaining" -eq 0 ] || { echo "WARNING: $remaining debugtraps not patched"; exit 1; }
