# AC6_recomp oracle qualification

This directory qualifies `AC6_recomp` only as a bounded control-flow, ABI and
capture oracle. It is not a product dependency and none of its source,
generated output, runtime libraries or retail inputs may enter the native tree
or package.

The pinned checkout is created outside this repository from an existing clean
clone:

```sh
git -C AC6_RECOMP_CLONE worktree add --detach AC6_ORACLE_WORKTREE \
  dcd41b7457fcac8242f8ef40de83d1719390d5af
```

Validate the full local qualification before a capture:

```sh
python3 tools/audit_ac6_oracle_manifest.py \
  analysis/oracle/ac6-recomp-dcd41b/manifest.json \
  --artifact-root . --oracle-root AC6_ORACLE_WORKTREE --xex PAL_DEFAULT_XEX
```

Each capture must use a named, hashed probe contract. The probe emits bounded
JSON Lines; normalize it before review:

```sh
python3 tools/normalize_ac6_recomp_trace.py \
  analysis/oracle/ac6-recomp-dcd41b/manifest.json mission01-frame \
  RAW.jsonl NORMALIZED.json --artifact-root .
```

The committed raw trace is a schema fixture, not an oracle observation. The
manifest deliberately says `capture_status=not-captured`; neither fixture nor
an unqualified local capture can close JF, JV, JP or JG. A real trace must
retain the manifest digest, oracle and XEX identities, probe digest, guest
addresses, ticks, normalized inputs, graphics state and output hashes.
