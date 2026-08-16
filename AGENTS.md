# AC6 (Xbox 360) scoped guidance

## Agent wait timeout

- Pour `wait_agent`, utiliser `timeout_ms=3600000` (1 heure) ou omettre ce paramètre puisque le défaut configuré vaut 1 heure ; ne jamais utiliser `900000`/15 min ni `300000`, sauf demande explicite.
- Never repeat unchanged `wait_agent` polling. After a timeout, wait again only when new status or information justifies it; otherwise report or suspend.
- Do not invent, expand, or continue work beyond the explicit request. Do not add unsolicited tasks or optional improvements.

## Scope, paths and resources

- Inherit the portfolio rules in `AGENTS.md`. This file only adds AC6-specific
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
  SHA-256, module and address. For the current PAL `default.xex`,
  `workspaces/ace-combat-6/ghidra-projects/ace-combat-6` is the canonical
  project used by `workspaces/ace-combat-6/ghidra-bridge.yaml` and by a fresh
  headless import. Treat
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
- For interactive retail observation, follow
  `workspaces/ace-combat-6/XENIA_WINE_ORACLE_HANDOFF.md`;
  it records the pinned Wine/Vulkan launcher, local `codex` profile and AZERTY
  keyboard route without promoting oracle use to parity evidence.

## Projet Codex multi-agent

- Sol est réservé exclusivement à l’agent racine; ne jamais le spawner, le
  configurer ni l’hériter pour un enfant. Le Sol racine planifie, architecture,
  traite le débogage difficile, délègue et décide; il n’implémente, ne recherche,
  ne relit, ne teste et n’effectue aucune autre exécution.
- Seul l’agent racine peut spawner.
- Si un sous-agent Sol semble nécessaire, arrêter et demander l’accord explicite
  de l’utilisateur pour cette seule tâche, jamais de façon permanente.
- Chaque spawn doit utiliser explicitement `fork_turns = "none"` et ne
  transmettre que le contexte propre à la tâche.
- Les sous-agents ne spawnent jamais d’autres agents.
- Tous les sous-agents, nommés ou dynamiques/non spécifiés, commencent avec
  `gpt-5.6-luna` et `reasoning_effort="xhigh"`. Si cette tentative reste
  incomplète, le racine peut lancer un dernier sous-agent par défaut avec
  `gpt-5.6-luna` et `reasoning_effort="max"`. Aucun autre modèle n'est autorisé
  pour les sous-agents.
- Flux préféré : Sol planifie → researcher enquête → coder
  implémente → reviewer relit → coder corrige les constats valides → Sol
  décide.
- Ne jamais remplacer silencieusement un modèle configuré devenu indisponible.

### Delegated task completion

- Give every worker explicit, enumerable acceptance criteria.
- After a worker returns, compare its result against every criterion.
- A partial worker result is not a completed task merely because tests passed.
- If the work is incomplete but directionally correct, prefer one targeted
  follow-up to the same worker over spawning a fresh agent.
- The follow-up must list only the unresolved criteria and must not request
  repetition of completed work.
- Spawn a fresh worker only when a clean context, independent review, or
  different role is materially useful.
- Allow one Luna/xhigh completion attempt and, only if it is incomplete, one
  Luna/max completion attempt per logical task. If still unresolved, return the
  evidence and unresolved criteria to root for reassessment; never spawn a third
  subagent.
- Sol keeps orchestration terse and does not produce long status analyses
  between worker attempts.
