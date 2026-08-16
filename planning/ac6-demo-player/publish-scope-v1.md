# AC6 publication scope v1

Branch `static-scenario-schema-microexec`, remote `origin`
(`https://github.com/ALavault/ac6-native.git`). No publication is authorized by
this document alone. `coherent_demo_product` requires explicit user approval.

## Scopes

`goal_direct_only` is the compact checkpoint scope: cycles 1754–1760, MCP v2,
FSM, demo-recomp IPC, vision sonde, runners and planning. It is explicitly
`non_buildable=true`: the CMake runtime and its host input library are omitted.

`coherent_demo_product` is the complete source/evidence closure. It includes:

- 166 clean files under `recompilation/ace-combat-6-demo/`;
- the three untracked `reconstruction/ac6-xbox360-host` files required by CMake;
- the 29-file v1/v2 `tools/emu_agent` package and its tests;
- all 162 durable `analysis/demo/*.json` evidence files used by the current
  demo validation corpus;
- reports, planning, vision configuration, examples and bridge patch support.

The demo tree is all untracked in the current worktree, so a partial commit
containing only IPC or only the reports cannot configure/build the product.
The CMake closure also requires the local `tools/audit_cpp_complexity.py`,
`tools/qualify_ac6_map_shaders.py`, target configs/tools/tests, and the external
toolchain prerequisites listed in the JSON. XEX/PAC/TBL and generated outputs
remain outside the publication set.

## Verification contract

Resolve every `files` entry and every `include_globs` entry from the repository
root, apply `absolute_exclusions`, and require the expected counts in the JSON.
No resolved path may match an exclusion. Scan the resolved set for secret
material (private keys, bearer tokens, credential files) before any staging.

The exclusion policy is absolute: build directories, generated/codegen/out
trees, caches, Python bytecode, logs, binaries/objects, XEX/PAC/TBL files and
secret-like files are never publishable.
