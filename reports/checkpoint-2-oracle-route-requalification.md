# Checkpoint 2 — requalification de la route oracle Mission 01

Date : 2026-08-13 (contrat R0)

## Résultat

Le profil capture historique `AC6_recomp@dcd41b7457f` parcourt un profil
neuf jusqu'au HUD de vol Mission 01. Deux exécutions au même binaire ont passé
les 96 étapes, sans fatal ni processus résiduel. Le harnais supprime uniquement
le nouveau segment `rexglue_memory_*` qu'il a lui-même créé et restaure
exactement l'inventaire `/dev/shm` initial.

Les deux runs reproduisent les mêmes entrées, les trois mêmes hashes de
registres invités et des captures identiques au HUD initial et après la fenêtre
de commandes. L'artefact qualifié est
`analysis/oracle/ac6-recomp-dcd41b/captures/mission01-hud-route/qualification.json`.
Cette lignée PAL est désormais **historique seulement** : elle ne peut produire
ni nouveau reçu, ni trace promouvable, ni fermeture de gate.

L'oracle comportemental actif est le binaire NTSC-U/J exact
`6eefba42…cbbbc`, `AC6_recomp@ab90b54713e5889f33eee1cc8681dae89fe83d1e`,
arbre `1e60427e…fd50`. Son identité durable est
`analysis/oracle/ac6-recomp-ab90b-us/identity.json`. La cible produit reste le
PAL canonique `acc302c1…bcde`; aucune preuve NTSC-U/J n'est une preuve PAL.

## Causes requalifiées

- `reports/cycle-548-atoi-fix-and-ghidra-bridge-reconciliation.md` :
  **reproduit**. Le thunk PAL `0x82382480` appelle le worker que la table CRT
  épinglée identifie à tort comme `strcpy_s`; le résultat `EINVAL=22` remplaçait
  le message `M350`. Le parseur décimal borné et son test restaurent `350`.
- Route historique avec deux `Left` au choix de sauvegarde : **superseded**.
  Le second `Left` revenait sur `NO`; un seul `Left` produit naturellement
  `type28 6→8→10`.
- Ancien override sale de `0x820F6180` : **indice**, puis **reproduit** contre
  le projet Ghidra canonique. Le corps PAL de 16 octets a le SHA-256
  `829ec6aa…25c9`; l'export passe de 10 644 à 10 645 fonctions sans autre
  changement.
- Stalls de campagne historiques antérieurs à ces corrections :
  **superseded** pour cette route. La transition `0→1→2`, le lancement et le
  HUD sont maintenant atteints sans intervention mémoire.

L'include `generated/ac6recomp_config.h` avant les macros PPC est un invariant
obligatoire : sans lui, `PPC_CALL_INDIRECT_FUNC` devient un `debugtrap`
inconditionnel dans un fichier hôte. Une garde de compilation empêche sa
régression.

## Dialogue vidéo et audio

Le fait qu'un dialogue de vidéo soit audible est cohérent avec l'oracle et
constitue un **indice d'observation** utile. Ce n'est pas encore une preuve du
chemin natif : ces runs utilisent `SDL_AUDIODRIVER=dummy`. Le rapport
`checkpoint-2-media-xma-contract.md` qualifie le décodage XMA du BGM, mais garde
explicitement ouverts le décodage ASF de `moviepack.bin` et la synchronisation
audio/vidéo.

## Frontière suivante

Le runtime Linux/Vulkan minimal de la lignée active est construit et O1 est
validé : profil stock, polls continus, trois `PRESENT` non noirs et arrêt
propre avec lignée vérifiée. La prochaine frontière est O2, capture deux fois
du corpus avec un contrôleur physique unique. `ac6.execution-trace.v3` est le seul format
inscriptible : il sépare oracle NTSC-U/J, marqueur, reçu v4, cible PAL et
producteur, puis ordonne six domaines (`input`, `simulation`, `objectives`,
`graphics`, `media`, `hashes`). La v2 reste lisible comme historique, jamais
promouvable. Le monde retail derrière l'ancien HUD reste noir et non rendu.

## Validation

- l'ancienne route `ay`/`az` reste une observation historique bornée ;
- le contrat courant est vérifié par `tools/audit_ac6_global_ladder.py` et
  `tools/tests/test_ac6_execution_trace_v3.py` ;
- aucune lane du checkpoint 2 n'est fermée par R0, O1 ou O2.
