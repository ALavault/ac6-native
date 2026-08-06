# AC6 (Xbox 360) scoped guidance

- Target is Xbox 360, never Xbox One. Preserve Xenon big-endian PPC, 64-bit
  registers/32-bit guest pointers, AltiVec/VMX128, Xenos, xboxkrnl, XAM and XMA.
- Keep AC6 in the active portfolio even when Pharaoh or AC5 is prioritized.
  Defer VNC/controller sessions by default; request one only for a named
  static evidence boundary that cannot be closed otherwise, and specify the
  exact input, expected artefact and time limit in the handoff.
- Use XenonRecomp/XenonAnalyse/XenosRecomp as deterministic evidence tooling;
  never edit generated output. Xenia is an oracle only.
- Generated C++ from a revision-pinned `AC6_recomp` checkout may be used as
  literal control-flow/ABI cross-match evidence only after qualifying the XEX
  SHA-256. Do not copy it into the native implementation, infer semantics from
  its generated names, or let its configured function starts override Ghidra
  boundaries and executed validation.
- Qualify every Ghidra result by project name as well as target ID, XEX
  SHA-256, module and address. For the current PAL `default.xex`,
  `ghidra-projects/ace-combat-6` is the canonical project used by
  `ghidra-bridge.yaml` and by a fresh headless import. Treat
  `ghidra-projects/ace-combat-6-corrected` as historical/needs-revalidation
  until its bytes are reconciled; never merge exports from both projects.
- For generic Xenon/Xenos, guest-memory and recompilation interpretation,
  consult the local, provenance-checked architecture catalog at
  `.tools/knowledge-base/architecture-v1/catalog.json`; it supports but never
  replaces binary-qualified AC6 evidence.
- Do not upload `DATA00.PAC`, `DATA01.PAC`, or another retail container at or
  above 512,000,000 bytes to ChatGPT. When external review needs bytes from a
  large container, provide a manifest of exact file-relative ranges
  (`offset`, `length`, purpose, source SHA-256) and package only the bounded
  locally extracted slices required by the question.
- Keep runtime hooks, kernel/XAM/XMA services and renderer divergence as
  explicit boundaries. Build and install with the AC6 command from root rules.
- `SDL_AUDIODRIVER=dummy` is the qualified audio configuration for AC6 Xvfb
  runs. Without it, headless startup may stall after one `PRESENT`; do not
  remove it from a headless harness or classify that stall as a guest/build
  regression without an audio A/B.
- For interactive retail observation, follow `XENIA_WINE_ORACLE_HANDOFF.md`;
  it records the pinned Wine/Vulkan launcher, local `codex` profile and AZERTY
  keyboard route without promoting oracle use to parity evidence.
