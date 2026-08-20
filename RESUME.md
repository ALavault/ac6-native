# Reprise autonome — AC6 démo PAL native

## Périmètre

Runtime canonique : `recompilation/ace-combat-6-demo`. Cible unique :
`demo-game-file/extracted/stfs-root/Default.xex`, SHA-256
`de917873f601e2a2208d75ab907e918ce941a42378d0d088705ecb4477405da8`,
Ghidra `ace-combat-6-demo`. Ne jamais mélanger retail, projet corrected ou C++
généré. Edge sert uniquement par son source épinglé ; ne pas lancer Edge/Wine.

Objectif global : cold boot → menu visible → mission jouable → succès et échec
endogènes, avec replay déterministe, capsules et readback guest non noir.
L’objectif n’est pas atteint.

## Checkpoint actuel

Les commits fonctionnels sont `2e6228c8` puis `aa9b0534` sur `main`, poussés.
Le second retire une écriture spéculative vers `KTHREAD+0x58`. Le seul
correctif causal conservé est le contrat RPTR : le CP écrit uniquement à
l’adresse passée à `VdEnableRingBufferRPtrWriteBack`, jamais à `ptr-0x3c`.

Le runtime accepte aussi les formes PM4 atteintes `0x61/0x62/0x63` et les
valeurs dynamiques qualifiées de `EVENT_WRITE_SHD`, avec tests ciblés.

Preuve canonique :
`analysis/demo/ac6-demo-ring-readback-frontier-v1.json`.

Résultat final reproductible :

- headless et Vulkan atteignent 3 036 ticks avec START 3 000/release 3 001 ;
- 23 threads guest, tous bloqués à la fin ;
- 2 899 `VdSwap`, frontbuffer `0x1374A000`, format 6, `1280×720` ;
- second ring `0x126CA000`, 0 soumission, RPTR/WPTR 0 ;
- Vulkan : 2 modules, 0 pipeline raster, 0 normal draw, 0 writeback guest,
  aucun screenshot et aucun RGB non nul ;
- frontend, mission et terminaux restent faux.

Les notifications `VdSwap` ne sont pas un visuel. Ne promouvoir aucun
screenshot tant qu’un pipeline/readback guest non noir n’est pas joint à un
état guest qualifié.

## Validation

Depuis la racine portfolio :

```bash
export TMPDIR=/fastdata/lavaulta/tmp
cmake --build workspaces/ace-combat-6/recompilation/ace-combat-6-demo/build-codegen-on -j16
SDL_AUDIODRIVER=dummy xvfb-run -a ctest --test-dir workspaces/ace-combat-6/recompilation/ace-combat-6-demo/build-codegen-on --output-on-failure
cmake --install workspaces/ace-combat-6/recompilation/ace-combat-6-demo/build-codegen-on --prefix "$PWD/workspaces/ace-combat-6"
test ! -e workspaces/ace-combat-6/bin/bin
```

Dernier résultat : build PASS, CTest 26/26, installation PASS.

Commande de corridor depuis `workspaces/ace-combat-6` :

```bash
export TMPDIR=/fastdata/lavaulta/tmp
SDL_AUDIODRIVER=dummy xvfb-run -a \
  recompilation/ace-combat-6-demo/build-codegen-on/ac6-demo-recomp probe \
  --store .build/ac6-demo-store-test-3 --until frontend --max-ticks 3036 \
  --trace <trace> --report <report> --backend headless \
  --input-at 3000,16,0,0,0,0,0,0,1 \
  --input-at 3001,0,0,0,0,0,0,0,1
```

Pour Vulkan, remplacer le backend, créer d’abord un répertoire de captures et
définir `AC6_DEMO_AUDIT_SCREENCAP_DIR`.

## Règles de travail

- Pas d’optimisation avant affichage natif du début de mission.
- Pas d’A/B par défaut ; seulement pour une ambiguïté causale explicite.
- Aucun CPJ, worker automatique ou sous-agent.
- Inconnues en trap, aucun résultat synthétique, aucun changement du généré.
- Préserver le worktree sale : les fichiers listés dans `STATE.md` sont
  préexistants et ne doivent pas être inclus sans qualification.

## Reprise unique

Lire `NEXT.md`. Le seul prochain gate est la première publication authentique
du WPTR après la seconde `VdInitializeRingBuffer(0x126CA000,16)`. Il faut
nommer le producteur PPC exact et distinguer : absence du write guest contre
write présent mais bridge CP défaillant. Ne poursuivre aucun autre axe avant
ce gate.
