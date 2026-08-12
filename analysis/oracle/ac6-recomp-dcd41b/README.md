# AC6_recomp oracle qualification

This directory qualifies `AC6_recomp` only as a bounded control-flow, ABI and
capture oracle. It is not a product dependency and none of its source,
generated output, runtime libraries or retail inputs may enter the native tree
or package.

The pinned checkout is created outside this repository from an existing clean
clone:

```sh
git -C AC6_RECOMP_CLONE worktree add --detach AC6_ORACLE_WORKTREE \
  dcd41b7457fcac8242f8ef40de83d1719390d5af
```

The immutable `manifest.json` qualifies the boundary-only reference build. Keep
that worktree separate from the capture overlay and validate it with:

```sh
python3 tools/audit_ac6_oracle_manifest.py \
  analysis/oracle/ac6-recomp-dcd41b/manifest.json \
  --artifact-root . --oracle-root AC6_ORACLE_WORKTREE \
  --patched-oracle-root AC6_BOUNDARY_WORKTREE --xex PAL_DEFAULT_XEX \
  --overlay-archive SIMDE_OVERLAY.tar \
  --generated-root AC6_ORACLE_WORKTREE/generated \
  --runtime-binary AC6_BOUNDARY_WORKTREE/out/build/linux-amd64-release/ac6recomp
```

The pinned SDK accidentally omits the vendored SIMDe x86 subtree. The manifest
therefore qualifies an immutable, include-only overlay by source commit, Git
tree and archive SHA-256. Extract that archive outside both repositories and
configure with the literal manifest recipe, replacing
`${SIMDE_OVERLAY_ROOT}` with the extracted root. The overlay is authorized only
for compiling this oracle; it does not alter the clean detached worktree and
has no product role.

The qualified codegen result comprises 56 files. Its file names and contents
are covered by `codegen.generated_tree_sha256`; the auditor also checks the
total file and byte counts. The generated directory remains ignored external
evidence and must never be copied into the native source tree or package.

The committed function-start list contains only boundaries proved against the
canonical PAL Ghidra project. Reproduce the boundary-only worktree with:

```sh
python3 tools/apply_ac6_oracle_boundary_corrections.py \
  analysis/oracle/ac6-recomp-dcd41b/manifest.json \
  AC6_ORACLE_WORKTREE/ac6recomp_config.toml \
  AC6_BOUNDARY_WORKTREE/ac6recomp_config.toml
git -C AC6_BOUNDARY_WORKTREE apply --unidiff-zero \
  PROJECT_ROOT/analysis/oracle/ac6-recomp-dcd41b/patches/remove-false-vertex-declaration-hook.patch
```

The small host patch removes the old `0x821DE7D0` vertex-declaration hook. The
qualified Ghidra contract proves that address is internal to `0x821DE7A8`, not
a device bind or independent ABI entry. Both modifications are oracle-only;
neither has a product role.

The capture worktree starts from the same detached commit with only the
boundary-corrected configuration dirty. Derive the portable runtime/capture
configuration and apply all 13 sealed host patches transactionally:

```sh
python3 tools/apply_ac6_oracle_patch_stack.py \
  analysis/oracle/ac6-recomp-dcd41b/patches/stack.json \
  AC6_CAPTURE_WORKTREE --artifact-root .
python3 tools/apply_ac6_oracle_patch_stack.py \
  analysis/oracle/ac6-recomp-dcd41b/patches/stack.json \
  AC6_CAPTURE_WORKTREE --artifact-root . --apply
```

Configure this capture host with the qualified SIMDe include overlay and the
explicit system Clang 21 compiler, then build codegen and the complete host.
`reproducibility-v1.json` seals the resulting source overlay, generated tree,
binary, CTests and smoke separately from the boundary manifest:

```sh
python3 tools/audit_ac6_oracle_reproducibility.py \
  --retail-root GAME_FILES \
  --architecture-catalog ARCHITECTURE_CATALOG \
  --reference-root AC6_ORACLE_WORKTREE \
  --runtime-root AC6_CAPTURE_WORKTREE \
  --preserved-checkout reference=AC6_DIRTY_REFERENCE \
  --preserved-checkout gapfill=AC6_DIRTY_GAPFILL \
  --simde-archive SIMDE_OVERLAY.tar
```

The bounded Linux smoke must place `timeout` inside `xvfb-run` and retain the
qualified dummy audio driver:

```sh
SDL_AUDIODRIVER=dummy xvfb-run -a \
  timeout --signal=INT --kill-after=3s 15s \
  AC6_RUNTIME_WORKTREE/out/build/linux-amd64-release/ac6recomp GAME_FILES
```

The current build initializes the qualified XEX, Vulkan device and 1280x720
swapchain, then stops at the recorded unresolved branch
`0x8234530C -> 0x8234524C`. This is a discovery frontier, not gate evidence.
The retail oracle presents and samples input/state at 30 Hz. Native comparison
holds each oracle sample for two 60 Hz simulation ticks; it must not report the
oracle as a 60 FPS source.

Each capture must use a named, hashed probe contract. The probe emits bounded
JSON Lines; normalize it before review:

```sh
python3 tools/normalize_ac6_recomp_trace.py \
  analysis/oracle/ac6-recomp-dcd41b/manifest.json mission01-frame \
  RAW.jsonl NORMALIZED.json --artifact-root .
```

The committed raw trace is a schema fixture, not an oracle observation. The
manifest deliberately says `capture_status=not-captured`; neither fixture nor
an unqualified local capture can close JF, JV, JP or JG. A real trace must
retain the manifest digest, oracle and XEX identities, probe digest, guest
addresses, ticks, normalized inputs, graphics state and output hashes.
