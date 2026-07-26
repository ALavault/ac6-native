# AC6 shared mode-owner receiver-dispatch audit

Date: 2026-07-15

## Question

Can the shared owner at `DAT_8293BA10` identify a concrete receiver for the
title-mode `receiver->virtual_0x20(3)` publication, or connect that
publication to NFIC `CutTerminate`?

## Exact static result

`scripts/TraceGlobalReceiverDispatches.java` traces only this literal
PowerPC data-flow shape within functions that directly reference the global:

1. load `DAT_8293BA10`;
2. load its receiver field at `+0x08`;
3. load receiver vptr at `+0x00`;
4. load vtable slot `+0x20`;
5. transfer through `CTR`.

Its run is retained in
`reports/receiver-global-owner-field8-dispatches.log`. It identifies seven
dispatch sites, each with the immediately preceding `r4 = 3` setup:

| Function | owner load | receiver load | slot load | CTR transfer |
| --- | --- | --- | --- | --- |
| `0x82198050` | `0x82198098` | `0x8219809C` | `0x821980B8` | `0x821980BC` |
| `0x82199D08` | `0x82199D3C` | `0x82199D40` | `0x82199D5C` | `0x82199D60` |
| `0x8219B208` | `0x8219B2D8` | `0x8219B2DC` | `0x8219B2F8` | `0x8219B2FC` |
| `0x8219B650` | `0x8219B698` | `0x8219B69C` | `0x8219B6B8` | `0x8219B6BC` |
| `0x821B32C8` | `0x821B32EC` | `0x821B32F0` | `0x821B330C` | `0x821B3310` |
| `0x821B3AD0` | `0x821B3B18` | `0x821B3B1C` | `0x821B3B38` | `0x821B3B3C` |
| `0x821B90B0` | `0x821B90BC` | `0x821B90C0` | `0x821B90DC` | `0x821B90E0` |

The decompilations for these entries are retained as
`reports/owner-dispatch-*.log`. Each reaches the same publication shape:

```c
receiver = *(void **)(DAT_8293BA10 + 0x08);
*(uint32_t *)(DAT_8293BA10 + 0x18) = 1;
*(uint32_t *)(DAT_8293BA10 + 0x1c) = 3;
if (receiver != NULL) receiver->vtable[0x20 / 4](receiver, 3);
```

Several sites are countdown or input paths; for example `0x82198050` first
invokes a task-local virtual `+0x48` callback on a logical-input condition,
then makes the same shared publication. This proves neither receiver type nor
the meaning of value `3` beyond the shared mode signal.

The audit does not find an implementation address because the receiver vptr
is read from runtime memory. The owner constructor initializes `+0x08` null;
the direct-global store audits still find no later static setter. Therefore
the seven static callers do not close a receiver vtable or method body.

## NFIC termination check

The direct reference query for the NFIC event dispatcher `0x8236B920` has one
static caller, `0x8236BBE0`, inside wrapper `0x8236BBD0`; its complete body is
only:

```c
FUN_8236b920(unaff_r31, unaff_r30, unaff_r29);
```

The wrapper supplies no typed successor selector, resource request, title
owner, or mission object. The dispatcher itself still obtains its event
receiver dynamically from its runtime context before virtual slot `+0x20`
dispatch. Thus this check adds no static edge from `CutTerminate` to a
post-CUT state.

Evidence:

- `reports/nfic-dispatch-direct-callers-followup.log`;
- `reports/nfic-dispatch-direct-caller-8236bbe0.log`;
- `reports/nfic-dispatch-state.log`;
- `reports/receiver-global-owner-field8-dispatches.log`.

## Native consequence

No native post-CUT successor is added. The Linux session must remain at
`scene_complete` after the decoded `CutTerminate` boundary; starting another
Scene group, a flight loop, or a title transition from this signal would be
unsupported.
