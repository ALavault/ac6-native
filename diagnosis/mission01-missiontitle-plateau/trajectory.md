# Update-vs-wall-clock trajectories (read-only, from existing run logs)

`ms=` is wall-clock (7200000 ms = 120 min matched log mtime span).

## Run B — `ac6-r596-fixed.log` (BOOT_PHASE_TRACE, buggy hooks, probe window 7.2M ms)

| ms (min)   | updates | Δupdates/slice |
|-----------:|--------:|---------------:|
| 100        | 0       | —              |
| 600100 (10)| 0       | 0              |
| 1200100(20)| 771     | +771           |
| 1800100(30)| 1680    | +909           |
| 2400100(40)| 1683    | **+3**         |
| 3000100(50)| 1687    | +4             |
| 3600100(60)| 1690    | +3             |
| 4200100(70)| 1691    | +1             |
| 4800100(80)| 1693    | +2             |
| 5400100(90)| 1694    | +1             |
| 6000100(100)|1697    | +3             |
| 6600100(110)|1699    | +2             |
| 7200000(120)|1700    | +1             |

max updates=1700; `presented_frames=1703 state=2`; mode_vptr stayed 820661fc (Opening); never transitioned (probe cutoff).

**Collapse point: update ≈1680, ms≈1.8M (30 min).** Rate drops from ~800/slice to ~2-3/slice = ~250×–400× collapse, sharply, at a specific update — not a gradual/uniform slowdown.

## Run A — `ac6-r594-wptr.log` (VD_TRACE, buggy hooks, probe window 7.2M ms)

- Reached MissionTitle: `r567 mode ordinal=8 vptr=82065064` at frame 1702, then froze/spun (updates 1701–1703).
- `vd swap presented` count = **1706**; `vd drain rejected decode_ok=0` = **2,872,451**.
- Present count (1706) ≈ update count (1703): updates are ~1:1 with presents → **present/frame-gate-bound**.

## Cross-run

Both runs plateau at updates≈1700–1703 / presents≈1703–1706. Run-to-run variance is only whether the final ~2–3 updates (the MissionTitle transition at frame 1702) squeak in before the probe window — a race Run A won and Run B lost. The plateau itself is stable across both.

## Per-update wall cost in the collapse band

updates 1680→1700 spanned ms 1.8M→7.2M = 5.4M ms / 20 updates ≈ **270 s per update** (vs <1 s/update before 1680). Even at the GPU-starved ~0.24 fps baseline (one present ≈4 s), 270 s/update is ~65× beyond one present — so the collapse is NOT one-present-per-frame starvation alone.
