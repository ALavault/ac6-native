# Ace Combat 6 static-analysis workspace

This ignored workspace is reserved for read-only preparation of the supplied
Xbox 360 disc image. Proprietary inputs and extracted content must remain local
and must not be committed or redistributed.

Start with `PREPARATION_REPORT.md`, then follow `IMPORT_PLAN.md`. The supplied
archive and extracted proprietary files are evidence inputs, never source-tree
deliverables.

The active NTSC-U/J retail product lives under
`recompilation/ace-combat-6-retail/`. Its `rexglue-oracle` and `native` profiles
are explicitly separated; ReXGlue is oracle-only. `reconstruction/ace-combat-6/`
remains a distinct historical target and is not merged into the retail product.
Gate state and evidence live in `reports/handoff/CURRENT.json`, `NEXT.md`,
`RESUME.md`, `STATE.md` and `EVIDENCE.md`.
