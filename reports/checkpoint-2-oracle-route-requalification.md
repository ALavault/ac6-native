# Checkpoint 2 — requalification de la route oracle Mission 01

Date : 2026-08-11

## Résultat

Le profil capture propre `AC6_recomp@dcd41b7457f` parcourt désormais un profil
neuf jusqu'au HUD de vol Mission 01. Deux exécutions au même binaire ont passé
les 96 étapes, sans fatal ni processus résiduel. Le harnais supprime uniquement
le nouveau segment `rexglue_memory_*` qu'il a lui-même créé et restaure
exactement l'inventaire `/dev/shm` initial.

Les deux runs reproduisent les mêmes entrées, les trois mêmes hashes de
registres invités et des captures identiques au HUD initial et après la fenêtre
de commandes. L'artefact qualifié est
`analysis/oracle/ac6-recomp-dcd41b/captures/mission01-hud-route/qualification.json`.

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

Le probe v1 ne publie que trois événements et mélange l'index de frame hôte au
digest graphique. Le checkpoint 3 reste donc ouvert : passer à
`ac6.execution-trace.v2`, fixer une horloge invitée de 3 600 ticks et comparer
la première divergence oracle↔natif. Le monde retail derrière le HUD reste
noir et n'est pas déclaré rendu.

## Validation

- route qualifiée deux fois : `ay` et `az`, 96/96 étapes, 27 captures par run ;
- CTest natif : 72/72 (71 passés, un skip retail qualifié) ;
- tests Python : 121/121 ;
- gate Mission 01 : `JF=pass`, aucun point ouvert ;
- audits artefacts, adresses et dérivations : pass ;
- frontière produit : 222 sources contrôlées, aucun marqueur oracle interdit ;
- `git diff --check` : pass sur le périmètre du checkpoint.
