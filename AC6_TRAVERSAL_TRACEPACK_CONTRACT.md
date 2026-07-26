# AC6 PAL traversal tracepack contract

This contract prepares a future non-interactive Xenia/XenonTests capture for
the retail traversal `0x8226A508 -> 0x8226ECB0`. It neither launches Xenia nor
changes a Xenia debugger configuration. A trace collector is external to this
repository and must create the JSON evidence before this validator can accept
anything.

Run the validator only on a captured file:

```bash
python3 tools/validate-ac6-traversal-trace.py tracepack.json \
  --output trace-validation.json
```

The tracepack must identify the qualified AC6 PAL XEX by SHA-256, declare
PowerPC big-endian and image base `0x82000000`, and contain the exact retail
call-site and traversal target. It must include the pre-call registers
`r3`, `r4`, `r5`, `r6`, `lr`, and `f1_bits`, plus at least one bounded memory
snapshot with pre/post SHA-256 values. Callback observations are optional
because the traversal's eligibility gates can skip all three direct callbacks;
when present they are restricted to `0x8222CCD0`, `0x8222B740`, or
`0x82227378` and carry the same complete register snapshot.

The validator deliberately does not infer aircraft identity, spawn, mission,
or position. Those claims require separately qualified pointer and gameplay
evidence.
