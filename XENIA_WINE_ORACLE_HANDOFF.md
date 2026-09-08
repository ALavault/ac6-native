# AC6 Xenia Wine oracle handoff for transcription Codex

This file is the operational handoff for a Codex thread that needs to observe
the retail AC6 oracle. Read `AGENTS.md` first. Xenia is an oracle only and is
never a shipped dependency or parity proof by itself.

## Qualified local route

- Target: AC6 Xbox 360 PAL, title ID `4E4D07D1`.
- Retail entry: `game-files/default.xex`.
- Retail XEX SHA-256:
  `acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde`.
- Emulator: official Xenia Canary Windows release `16e1eb8`, built
  2026-07-17.
- Xenia executable SHA-256:
  `c52d27f9a115c036257efbedd91006e74964e0c12aebb09b0c1dd93a31280f9a`.
- Host route: Wine 10.0, Vulkan/FBO, XAudio2.
- Runtime service: `ac6-xenia-wine-gui.service`.
- Launcher: `scripts/launch_xenia_ac6_wine.sh`.

Do not substitute the native Linux Xenia baseline for this route. On this
host, the native build launched the module but remained black. The qualified
interactive route is the pinned Windows build through Wine with Vulkan.

## Commands

Run from the repository root:

```bash
workspaces/ace-combat-6/scripts/launch_xenia_ac6_wine.sh check
workspaces/ace-combat-6/scripts/launch_xenia_ac6_wine.sh launch
workspaces/ace-combat-6/scripts/launch_xenia_ac6_wine.sh status
workspaces/ace-combat-6/scripts/launch_xenia_ac6_wine.sh stop
```

`status` prints `status=running` for an active transient unit and
`status=inactive` when the unit has been collected after a normal stop.  The
latter is not a preflight failure and must not trigger a relaunch by itself.

`check` must report exactly:

```text
status=ready
release=16e1eb8
renderer=vulkan
service=ac6-xenia-wine-gui.service
```

The launcher uses the live GNOME display, an isolated Wine prefix, Vulkan,
the AC6-only ground fix and keyboard emulation. If preflight fails, classify
the route rather than silently changing the release, renderer, prefix or XEX.

## Local profile and save

- Profile name: `codex` (`user-confirmed`).
- Profile XUID: `E030000042B27D70` (`dynamic`, selected in slot 0 by the
  current Xenia configuration).
- Profile root:
  `.tools/xenia-canary-windows/16e1eb8/app/content/E030000042B27D70/`.
- AC6 save root:
  `.tools/xenia-canary-windows/16e1eb8/app/content/E030000042B27D70/4E4D07D1/00000001/sav_acecombat6/`.

The profile and save are local private runtime state. Preserve them. Do not
commit, package, redistribute, rename or overwrite them. A profile file or
save file is not a Xenia savestate and does not prove a precise gameplay
epoch.

## AZERTY keyboard map

Xenia must have window focus. The keyboard emulates controller port 0 with
`hid = "winkey"` and `keyboard_mode = 1`.

| Xbox 360 input | AZERTY key |
| --- | --- |
| Start | Return |
| Back | Backspace |
| A / confirm | `K` |
| B / cancel | `I` |
| X | `J` |
| Y | `U` |
| Left stick | `Z Q S D` |
| Right stick | Arrow keys |
| D-pad | Numpad `8 4 2 6` |
| Left / right trigger | `A` / `E` |
| Left / right shoulder | `R` / `T` |
| Left / right stick click | `F` / `H` |
| Guide | `G` |

Do not revert this to the generated QWERTY defaults. In particular,
`keyboard_mode = 0` disables all emulated controller input even though Xenia
still logs the WinKey binding table.

## Executed evidence and claim boundary

- The launcher preflight passes with the pinned executable and configuration.
- Xenia logs `Loading module GAME:\\default.xex` and
  `KernelState: Launching module`.
- The window identifies `ACE COMBAT 6 <Vulkan - FBO - XAudio2>`.
- Vulkan selects the local NVIDIA RTX PRO 4000 Blackwell and creates a
  1280x720 swapchain.
- The startup sequence renders rather than remaining black.
- The user confirmed on 2026-07-17 that AC6 is playable with the configured
  keyboard route.

Confidence for the interactive launch and keyboard route is `dynamic`. This
does not prove native parity, mission-specific timing, resource identity,
camera parity, save determinism or a reproducible savestate. For a
transcription claim, record the binary-qualified observation, exact profile,
visible epoch, input edge and resulting frame or debugger state. Keep raw
retail data, profile contents, saves and memory dumps out of reports and
archives.

Related evidence:

- `reports/cycle-97-xenia-wine-startup-sequence.md`
- `AC6_XENIA_ENTRY9_ORACLE_AUDIT.md`
- `scripts/run_xenia_ac6_oracle_baseline.sh` (bounded native-Linux launch
  baseline only; not the qualified interactive route)
