# AC6 Demo — strict XenonRecomp product

This directory is the isolated C++20/Linux x86-64 product boundary for the
Xbox 360 demo. It is qualified only for:

* `Default.xex`, SHA-256
  `de917873f601e2a2208d75ab907e918ce941a42378d0d088705ecb4477405da8`;
* the nine companion files listed in `config/demo-identity.json`.

The demo is the exclusive active AC6 product target. The PAL retail target,
its modified worktree, traces and Ghidra project are frozen and separate; they
are never accepted as demo evidence or runtime inputs.

Retail files, SDKs, Ghidra databases, ReXGlue, the XenonRecomp checkout and all
generated C++ stay outside this source tree. `tools/build_demo.py` obtains the
pinned XenonRecomp revision, applies the source patches, generates a function
and import manifest from the supplied XEX, and writes all products under the
build directory. Each generated translation unit is syntax-checked and
compiled to a build-only object, then combined into one relocatable guest
object to catch cross-TU definition errors. CMake links that object only when
`AC6_DEMO_ENABLE_CODEGEN=ON`; generated sources and objects remain build-only
and are never versioned or installed as standalone files. The 238 qualified imports get
build-only records. The 228 callable imports have a local fail-closed trap
which records module, ordinal, guest LR and tick when reached; the 10
XenonRecomp `kVariable` exports remain data records for the future guest
import mapper and are fail-closed until that boundary is implemented. The
trap is a build-only verification aid, not the native kernel/XAM
implementation.

## Canonical Ghidra import

From the portfolio root, create the ignored canonical project and the durable
text manifests with Ghidra 12.1.2:

```sh
python3 workspaces/ace-combat-6/recompilation/ace-combat-6-demo/tools/import_ghidra_demo.py \
  --xex workspaces/ace-combat-6/demo-game-file/extracted/stfs-root/Default.xex
```

The recipe refuses an existing project rather than merging it. It requires
project `ghidra-projects/ace-combat-6-demo`, module `Default.xex`, language
`PowerPC:BE:64:Xenon`, the pinned XEX identity and loader/version metadata.
Projects using `PowerPC:BE:64:A2ALT-32addr` are historical and must never be
merged into this manifest. Reproducibility requires two fresh imports in
different parent directories with byte-identical normalized manifest and
journal outputs.

The exhaustive boundary atlas is generated only from that qualified manifest,
the qualified `.pdata` bytes and `confirmed-chunks.toml`:

```sh
export TMPDIR=/fastdata/lavaulta/tmp
python3 workspaces/ace-combat-6/recompilation/ace-combat-6-demo/tools/build_static_decomp_atlas.py \
  --xex workspaces/ace-combat-6/demo-game-file/extracted/stfs-root/Default.xex \
  --xex1tool workspaces/ace-combat-6/.build/xex1tool/xex1tool \
  --manifest workspaces/ace-combat-6/analysis/demo/ac6-demo-ghidra-manifest.json \
  --output workspaces/ace-combat-6/analysis/demo/ac6-demo-static-decomp-atlas-v1.json
```

The writer refuses output collisions and emits canonical sorted JSON. An
optional `--semantics` export can enrich only exact address/size/hash matches;
it cannot create or resize a function. `coverage.complete` means every `.text`
byte is classified; semantic success and RTTI/indirect closure remain separate
gates.

Run `ExportDemoStaticSemantics.java` read-only with `analyzeHeadless -process
Default.xex -noanalysis` against each fresh project, then pass its atomic JSON
output through `--semantics`. The exporter hashes normalized pseudocode and
records direct edges; auto-generated symbols retain `confidence=unknown` and
are never written back to Ghidra.

## Build

```sh
cmake -S workspaces/ace-combat-6/recompilation/ace-combat-6-demo \
  -B workspaces/ace-combat-6/recompilation/ace-combat-6-demo/build -G Ninja \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build workspaces/ace-combat-6/recompilation/ace-combat-6-demo/build --parallel 12
SDL_AUDIODRIVER=dummy xvfb-run -a ctest --test-dir \
  workspaces/ace-combat-6/recompilation/ace-combat-6-demo/build --output-on-failure
```

The generator is opt-in and requires a locally supplied XEX:

```sh
python3 workspaces/ace-combat-6/recompilation/ace-combat-6-demo/tools/build_demo.py \
  --xex /path/to/Default.xex \
  --build-root workspaces/ace-combat-6/recompilation/ace-combat-6-demo/build/codegen \
  --ghidra-manifest /path/to/qualified/demo-ghidra-chunks.json \
  --xex1tool /path/to/xex1tool
```

The optional JSON manifest contains only inner chunks qualified against this
XEX; `.pdata` coverage and the versioned confirmed chunks are added by the script.
It fails closed on an identity mismatch, a missing import tool, an unsupported
instruction, or any function-boundary/switch diagnostic. The pinned generator
and its source patches are build inputs, not runtime dependencies.

The default build parallelism is 12 jobs. The generated XenonRecomp tool build
uses the same default; set `AC6_DEMO_BUILD_JOBS` only when a constrained host
needs a lower value.

Generic ABI seams that XenonRecomp exposes (context layout, endian helpers and
instruction hook call sites) are reusable under the upstream license and are
adapted in `tools/ppc_context_adapter.h`. Game-specific generated C++ is not
copied into the product. The native runtime owns the actual memory, timebase,
reservation, VMX and graphics implementations.

The current product does not claim `supported=yes`: the runtime surfaces are
implemented as strict boundaries and the six acceptance lanes remain open
until the generated guest has been qualified end to end.

## Executable contract

```text
ac6-demo-recomp import <directory> [--store <directory>]
ac6-demo-recomp play [--store <directory>] [--trace <file>] [--backend headless|vulkan]
ac6-demo-recomp replay <AC6RTPLY-v4> [--store <directory>] [--backend headless|vulkan] \
  [--xam-movie-replay <file>]
ac6-demo-recomp probe --store <directory> --until frontend|mission|terminal \
  --max-ticks N --trace <file> --report <file> --backend headless|vulkan \
  [--input-at tick,buttons,lt,rt,lx,ly,rx,ry,connected]... \
  [--xam-movie-record <file>|--xam-movie-replay <file>] [--atlas <file>]
```

`play` starts at the qualified XEX entry and reports only completed guest ticks;
it does not claim that a frontend was reached. `replay` parses every complete
input frame, executes the same number of guest ticks, and refuses the trace
unless the generated trace is byte-identical; AC6RTPLY v2/v3 and any XEX
identity other than the qualified demo are rejected. A codegen-OFF build
refuses `play` and `replay` before reading the store or trace. No command reads
retail paths, uses a network connection, or synthesizes a mission result.
With `--xam-movie-replay`, `replay` seals the movie before guest start and
refuses the first XAM divergence or unconsumed event; replay mode never falls
back to HID.
`probe` emits the versioned `ac6-demo-frontier-report/v1` with deterministic
indirect edges, import calls, register snapshots and scheduler wait records.
Each optional `--input-at` frame is applied before that guest tick and recorded
in the output `AC6RTPLY-v4`; unspecified ticks are connected neutral frames.
Ticks must be unique and lower than `--max-ticks`, so no physical controller or
host-input fallback participates in a probe.

## Dynamic evidence contracts

`AC6RTPLY-v4` remains the per-tick session replay. It is not a movie of import
calls. `ac6.xam-input-movie.v1` independently records every
`XamInputGetState` observation and strict replay rejects the first divergent
ordinal, caller, user, flags, pointer nullity or event count before any host
input fallback.

`probe --atlas` enables the generated-function entry hook only for that run and
publishes an atomic `ac6-demo-reachability-atlas/v1`. It requires a recorded or
replayed XAM movie, aggregates first/last tick and counts by exact generated
entry address, and never changes guest memory or consults HID during replay.

Three additional outputs have separate, installed schemas under
`config/schemas/`:

- `ac6-demo-milestone-digest/v1` joins a persistent guest-owned transition to
  the post-transition Xenos batch and 1280×720 readback;
- `ac6-demo-reachability-atlas/v1` stores the low-overhead complete-replay
  function/import/indirect-edge census;
- `ac6-demo-corridor-capsule/v1` stores one bounded frontier with registers and
  hashes of exact guest-memory ranges.

All three schemas pin the demo XEX and canonical Xenon Ghidra identity. They do
not make a frontend or mission claim by their presence alone.
