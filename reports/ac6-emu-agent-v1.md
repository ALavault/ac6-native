# AC6 `emu-agent/v1` — backend Xenia

État : implémenté, exécutable en mode diagnostic borné. La cible reste la
démo PAL `Default.xex` SHA-256
`de917873f601e2a2208d75ab907e918ce941a42378d0d088705ecb4477405da8`.

## Ce qui est livré

- `tools/emu_agent/protocol/` : canonicalisation, validation et hashes de
  `emu-agent/v1` / `emu-agent-result/v1`, y compris les événements
  `at.clock=xam_input_poll` avec état de contrôleur borné.
- `tools/emu_agent/runner/` : primitive `run_episode`, runner CLI et garde de
  sécurité.
- `tools/emu_agent/backends/xenia/` : identité AC6/Xenia Edge, matrice de
  capacités et backend fail-closed.
- `tools/emu_agent/exploration/ac6.py` : variantes neutre, pitch positif,
  négatif, dead-zone, saturation et décalage de poll, seedées et limitées à
  8–16 cas.
- `tools/emu_agent/artifacts/` : références et tranches explicitement bornées;
  aucune lecture arbitraire MCP.
- `tools/emu_agent/mcp_server/` : six seuls outils macro en stdio :
  `emu_capabilities`, `emu_run_episode`, `emu_compare_episodes`,
  `emu_branch_episode`, `emu_minimize_reproducer`, `emu_inspect_artifact`.
- `examples/emu-agent/` : replay, exploration et edge-input pitch.
- `tests/test_emu_agent_ac6.py` : six tests ciblés.

## Identités

| Élément | Valeur qualifiée |
|---|---|
| Git AC6 | `d7ca71b8734d8b4984ab658d34f69316342552f1` |
| Ghidra | `ace-combat-6-demo`, `PowerPC:BE:64:Xenon` |
| image base | `0x82000000` |
| Title / Media ID | `4E4D87E6` / `565E01A0` |
| Xenia Edge source | `e4b13738c3c461b2c06241fa3f54b5a669b6a304` |
| Xenia Edge AppImage | release `60ff861`, SHA-256 `c2cac2a029ce0d44a71c4e919fd71c702654079023b63fd669472ba3cd78b828` |
| configuration | absente dans cette campagne (`null`), jamais devinée |
| profil | aucune copie créée; la source reste intacte |

Le bridge Edge local possède les hooks génériques `AgentBridge::SubmitAction`,
`InputSystem::GetState` et `OnCompletedPresent`, mais aucun transport local
qualifié pour appeler `SubmitAction` et publier l’observation au safe point.
Le backend ne lance donc pas Xenia et n’injecte rien.

## Résultat et limites

Une requête Xenia valide produit un reçu `emu-agent-result/v1` avec
`status=stopped`, `stop_reason=xenia-transport-unavailable`, zéro progression
guest et `positive_control_seen=false`. Les événements sont néanmoins
canonicalisés et hashés; leur état final porte uniquement l’identité et la
matrice de capacités. `reproducibility.policy=diagnostic` est obligatoire pour
les exemples.

Les quatre contrôles pitch (neutre, positif, négatif, poll décalé) ne sont pas
promus : aucun n’a atteint XAM dans ce cycle. Il n’existe donc ni première
divergence retail/native, ni premier lecteur, ni consommateur joueur/enfant
clos. Les claims gameplay, pixel, audio, mission et parité restent interdits.

## Validation exécutée

```text
python -m compileall -q tools/emu_agent                 PASS
python -m pytest -q tests/test_emu_agent_ac6.py        6 passed
python -m tools.emu_agent.runner --safe \
  examples/emu-agent/ac6-pitch-replay.json              PASS (fail-closed receipt)
```

Le rapport JSON associé est
`reports/ac6-emu-agent-v1.json`. Les payloads retail, profils, saves, dumps,
traces et shaders ne sont pas suivis.

## Plus petite expérience suivante

Ajouter un canal local authentifié et allowlisté vers le bridge Edge épinglé,
sans modifier Xenia dans cette livraison : soumettre une action par poll,
attendre le safe point `XE_SWAP`, publier un événement XAM et fermer un seul
replay neutre. Rejouer ensuite les quatre contrôles depuis des copies isolées
du profil; seulement après liveness positive, raccorder le profil
`ac6_input_pitch_v1` et le contrat tracepack.
