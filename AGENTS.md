# AC6 (Xbox 360) scoped guidance

## Agent wait timeout

- Do not invent, expand, or continue work beyond the explicit request. Do not add unsolicited tasks or optional improvements.
- Pour chaque gate, favoriser et épuiser d’abord la valorisation statique — projet Ghidra canonique qualifié, XenonAnalyse, XenonRecomp/XenosRecomp et artefacts existants. N’autoriser une valorisation runtime que si une ambiguïté causale nommée reste ouverte ; fixer alors l’entrée, les observables, la fenêtre et le `done_when`, sans trace globale ni A/B par défaut.
- Pour la démo PAL qualifiée, le projet Ghidra canonique est `workspaces/ace-combat-6/ghidra-projects/ace-combat-6-demo`. Le projet PAL retail `workspaces/ace-combat-6/ghidra-projects/ace-combat-6` reste une cible distincte ; ne pas mélanger leurs exports ni leurs preuves.
- Do not run an A/B by default. Use one only when a named causal ambiguity
  cannot be resolved by a targeted trace and guest-state evidence; record the
  single differing input and the decision it is meant to settle.
- Until the native runtime visibly reaches the start of mission gameplay, do
  not optimize it. Make only correctness and execution-progress changes.

## Scope, paths and resources

- Inherit the portfolio rules from the portfolio-root `AGENTS.md` when one is
  present; none exists today, so this file stands alone. It adds AC6-specific
  constraints. Run commands from the portfolio root; every path below is
  portfolio-root-relative.
- Before adding analysis or build machinery, use the available procedures in
  `docs/native-recompilation-tools.md`, the architecture catalog at
  `.tools/knowledge-base/architecture-v1/catalog.json`, and the scripts under
  `workspaces/ace-combat-6/tools/` and `workspaces/ace-combat-6/scripts/`.
  Reuse their structured outputs and focused tests instead of duplicating them.
- Resume from `reports/handoff/CURRENT.json`, its AC6 source report and working
  set. Use `workspaces/ace-combat-6/XENIA_WINE_ORACLE_HANDOFF.md` only for a
  named interactive boundary.

## Target boundaries

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
  SHA-256, module and address. For the qualified PAL **demo** `Default.xex`,
  `workspaces/ace-combat-6/ghidra-projects/ace-combat-6-demo` is canonical.
  The separate retail PAL `default.xex` uses
  `workspaces/ace-combat-6/ghidra-projects/ace-combat-6`; it must not be mixed
  with demo evidence. Treat
  `workspaces/ace-combat-6/ghidra-projects/ace-combat-6-corrected` as historical/needs-revalidation
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
  explicit boundaries. The canonical native commands from the portfolio root
  are `cmake --build workspaces/ace-combat-6/reconstruction/ace-combat-6/build
  -j16`, `SDL_AUDIODRIVER=dummy xvfb-run -a ctest --test-dir
  workspaces/ace-combat-6/reconstruction/ace-combat-6/build
  --output-on-failure`, and `cmake --install
  workspaces/ace-combat-6/reconstruction/ace-combat-6/build --prefix "$PWD"`.
  After installation, require `test ! -e bin/bin`.
- `SDL_AUDIODRIVER=dummy` is the qualified audio configuration for AC6 Xvfb
  runs. Without it, headless startup may stall after one `PRESENT`; do not
  remove it from a headless harness or classify that stall as a guest/build
  regression without an audio A/B.
- For a native Xenia Edge oracle capture, use
  `scripts/run_xenia_edge_native.sh`, `SDL_AUDIODRIVER=dummy` and the pinned
  Edge profile. Its XEX must be
  `demo-game-file/extracted/stfs-root/Default.xex`, SHA-256
  `de917873f601e2a2208d75ab907e918ce941a42378d0d088705ecb4477405da8`;
  `game-files/default.xex` is retail and is forbidden for the demo oracle.
  A configured XUID alone is insufficient: before launching,
  verify that its corresponding account exists under the profile content root
  and that Edge will not show its profile-creation dialog. Inject gameplay
  input only after that check. If GPU tracing is requested, create the exact
  output directory first; if the pinned release emits no trace files, record
  that tooling limitation and do not repeat the same capture.
- For interactive retail observation, follow
  `workspaces/ace-combat-6/XENIA_WINE_ORACLE_HANDOFF.md`;
  it records the pinned Wine/Vulkan launcher, local `codex` profile and AZERTY
  keyboard route without promoting oracle use to parity evidence.

## Heavy-job resource safety

- Before running Ghidra/headless analysis, an emulator, a full parallel build,
  bulk extraction or a long test, inspect current host/cgroup memory. Run only
  one such job at a time.
- Run every heavy job in its own transient user cgroup with a wall-clock limit.
  Start from this wrapper and adjust limits only from observed capacity; never
  remove them:

  ```sh
  systemd-run --user --scope --unit=<unique-job> \
    -p MemoryHigh=16G -p MemoryMax=24G -p TasksMax=128 \
    timeout --signal=TERM --kill-after=30s <duration> <command>
  ```

- Give Java tools an explicit maximum heap below `MemoryHigh`. A resource-limit
  failure must fail the current gate, not the agent session.
- Redirect complete stdout/stderr to `artifacts/<gate>/` and return only a
  compact summary. Write the exit-status marker only after the process exits;
  if it is absent, treat the run as interrupted, never successful.
- Monitor memory while the job runs and stop it before its cgroup limit is
  exhausted. Never disable `systemd-oomd` or rely on swap as the safeguard.
- Before another heavy gate, checkpoint and rotate the agent session after
  three closed gates or 24 hours, whichever comes first.


## Stratégie d’investigation

Utilise d’abord toute information statique disponible pour réduire l’espace
des hypothèses et préparer les tests.

Optimise en priorité le nombre de décisions modèle nécessaires :
regroupe les lectures, extractions et validations déterministes dans des scripts
ou commandes batchées, et ne retourne au modèle qu’un rapport compact.

Une exécution dynamique reste autorisée lorsqu’elle constitue le moyen le plus
direct de départager des hypothèses encore compatibles. Dans ce cas, prépare
l’instrumentation statiquement, collecte toutes les observations compatibles
dans une seule exécution et ne retourne au modèle qu’après la fin de
l’expérience ou lorsqu’une décision réelle est nécessaire.

Le runtime n’est pas interdit. Les boucles observation-décision inutilement
granulaires le sont.
