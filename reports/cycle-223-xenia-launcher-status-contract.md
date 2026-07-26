# AC6 cycle 223 — contrat d'état du launcher Xenia/Wine

## Portée

Le launcher local AC6 utilise `systemd-run --user --collect`. Après un arrêt
normal, l'unité transitoire est donc retirée par systemd. L'ancien sous-mode
`status` appelait `systemctl status` directement et retournait alors une erreur
« Unit ... could not be found », même si aucun oracle n'était actif.

Ce cycle ne lance pas Xenia, Wine, Xenia Canary, VNC, GPU ou session humaine.
Il ne touche ni au XEX, ni au profil privé, ni aux sauvegardes.

## Contrat corrigé

`scripts/launch_xenia_ac6_wine.sh` retourne désormais une sortie structurée :

- unité active : `status=running` ;
- unité absente après collecte : `status=inactive` ;
- `stop` sur une unité déjà collectée : `status=inactive`, sans erreur ;
- `stop` sur une unité active : `status=stopped` après l'arrêt.

La préflight `check` reste inchangée et valide le binaire Canary épinglé, le
renderer Vulkan, le profil et le mapping AZERTY. Un statut `inactive` ne
justifie pas un relancement automatique : toute observation Xenia reste une
action d'oracle explicitement bornée.

## Validation

Exécuté :

```bash
bash -n workspaces/ace-combat-6/scripts/launch_xenia_ac6_wine.sh
uv run pytest -q tests/test_tools/test_ac6_xenia_launcher_status.py
workspaces/ace-combat-6/scripts/launch_xenia_ac6_wine.sh status
git diff --check
```

Résultat : **3/3 PASS**. Le statut local observé est `inactive`, sans processus
Xenia ou Wine résiduel.
