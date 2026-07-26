# AC6 cycle 261 — MATE request lifecycle and default draw context

## Scope and identity

- target: `ac6-xbox360-pal`;
- module: `default.xex` / `ACE6_X360.exe`;
- XEX SHA-256:
  `acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde`;
- canonical Ghidra project:
  `workspaces/ace-combat-6/ghidra-projects/ace-combat-6`;
- method: Ghidra `analyzeHeadless -readOnly -noanalysis` plus the qualified
  retail MATE/NSXR corpus from cycle 260.

This pass closes the cycle-260 search for a post-constructor writer of
`request+0x08` for the request family built by `0x82363F58`. It does not claim
that every AC6 draw request has this layout, and it does not claim a host draw
was emitted.

## Constructor and material route

The active path at `0x82362A38` selects a MATE material, constructs the request
at `0x82362C4C`, and submits the material/request pair to `0x8233ED10`:

- `0x82363F70` stores the selected material at `request+0x24`;
- `0x82363F84` stores literal zero at the independent field `request+0x08`;
- material flags route the request to a direct or sorted request manager.

The qualified retail corpus contains:

| material flags | records | route |
| --- | ---: | --- |
| `0` | 1,915 | direct list |
| `1` | 518 | sorted list unless the global override selects direct |
| `3` | 143 | direct list |

The total is 2,576 material records. Every `material+0` key is nonzero.

## Both queue variants preserve `request+0x08`

The direct enqueue at `0x823461B8` mutates only the previous request's `+0x04`
link and the manager tail. Its consumer at `0x82346208` obtains the head and
immediately dispatches vtable byte slot `+0x14`.

The sorted enqueue at `0x823463F0` stores the request pointer and sort float in
an external eight-byte record. Its consumer at `0x82346460` reloads that
request pointer and immediately dispatches the same slot `+0x14`.

Neither lifecycle contains a write to `request+0x08`. For this qualified
family, the constructor's zero therefore reaches draw method `0x82364980`
unchanged.

## Meaning of the two keys

Draw method `0x82364980` loads `request+0x08` at `0x82364B44`, resolves it via
registry `0x828CCB80`, invokes ShaderContext byte slot `+0x28`, then reaches
three calls to indexed draw `0x821DF2C0`.

Zero is a valid entry-163 NSXR key: container 0, ordinal 0. By contrast, none
of the 2,576 retail MATE `material+0` keys is zero. The bounded conclusion is:

```text
material+0 = nonzero per-material ShaderContext key
request+0x08 = zero default draw ShaderContext key for this request family
```

The keys are intentionally distinct on this path. This does not assign a
gameplay name to either context and does not imply that key zero is a null or
missing context.

Evidence:

- `artifacts/ac6-cycle261-request-queue-chain.log`;
- `artifacts/ac6-cycle261-request-manager-references.log`;
- `artifacts/ac6-cycle261-sorted-request-consumer.log`;
- `artifacts/ac6-cycle261-sorted-request-sort.log`;
- `artifacts/ac6-cycle261-request-manager-frame-transition.log`;
- `artifacts/ac6-cycle261-sorted-request-manager-vtable.log`;
- `artifacts/ac6-cycle261-mate-request-route-summary.json`;
- `artifacts/ac6-cycle261-mate-request-key-validation.log`.

## Native capture boundary and codegen preflight

At `0x82364B44`, immediately before the request-key lookup:

- `r30` is the request pointer;
- `r31` is the device pointer;
- `request+0x24` is the selected material;
- `material+0` is the per-material key;
- `request+0x08` is the default draw-context key.

This is the smallest qualified XEX boundary for publishing the tuple into the
existing AC6Recomp `CaptureDrawCall` path. The capture must be consumed once by
the next compatible guest draw and must not create a second event sink.

No hook was added in this cycle. ReXGlue generation is not currently
reproducible on this host:

1. the tracked Linux preset requests absent `clang-20/clang++-20`;
2. overriding it with installed Clang 21 reaches the next exact dependency
   failure: missing `gtk+-3.0` development metadata.

Evidence:

- `artifacts/ac6-cycle261-rexglue-codegen-preflight.log`;
- `artifacts/ac6-cycle261-rexglue-codegen-clang21-preflight.log`.

Generated output was not edited. No package was installed and no unrelated
dirty AC6Recomp work was changed.

## Validation

- `VerifyMateDrawRequestKeyLifecycleContracts.java`: **50/50 assertions pass**;
- retail route summary: **2,576/2,576** records classified, zero zero-valued
  MATE keys;
- Ghidra operations were headless and read-only;
- no Xenia, GUI, VNC or human action was used;
- no native runtime source changed, so the last full AC6 native result remains
  **44/44 PASS** from cycle 256.

## Remaining boundary

```text
MATE_KEY_TO_REQUEST_SHADER_KEY_DISTINCT_CONFIRMED
REQUEST_DEFAULT_CONTEXT_TO_GUEST_INDEXED_DRAW_CONFIRMED
MATE_IDENTITY_TO_NATIVE_GUEST_DRAW_CAPTURE_NOT_IMPLEMENTED
HOST_DRAW_EMITTED_NOT_OBSERVED
```

The next autonomous implementation step is the narrow capture hook at
`0x82364B44`, after the codegen toolchain is made reproducible. A separate
backend marker is then required to distinguish `guest_draw_captured`,
`host_issue_called`, `backend_success` and `host_draw_emitted`.
