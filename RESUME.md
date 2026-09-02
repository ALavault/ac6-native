# AC6 retail NTSC-U/J — reprise Gate 2 runtime natif

Reprendre depuis `recompilation/ace-combat-6-retail`.

Lire d'abord :

- `reports/handoff/CURRENT.json`;
- le report r168 cité comme `source_report`;
- `NEXT.md`;
- `STATE.md` et `EVIDENCE.md` seulement pour une question historique nommée.

## Frontière active

`VdQueryVideoMode` (r169), `VdQueryVideoFlags`/`VdGetCurrentDisplayGamma`/
`VdGetCurrentDisplayInformation` (r170) et `XGetVideoMode` (r171) sont
fixés. Candidat le plus prometteur actuellement identifié, non implémenté :
`XGetGameRegion` (3 sites d'appel réels, ex. `0x821babdc`) — sa valeur de
retour est stockée puis relue et comparée à plusieurs constantes précises
qui contrôlent un vrai branchement (potentiel bug de détection de région).
Analyser ses 3 sites avant d'implémenter. `XGetAVPack`/`XGetLanguage`
(1 site chacun) ne sont pas encore vérifiés.

Ne pas supposer qu'un import est un remplissage de structure sans lire ses
sites d'appel réels (r170 a infirmé cette hypothèse pour `VdQueryVideoFlags`).

Reste ouvert, non implémenté : `VdGetCurrentDisplayInformation` struct+0x05
(champ booléen réel, confirmé à 2 sites d'appel, valeur correcte non
tracée).

## Environnement de session

Si `.tools/xenonrecomp-source`, `.tools/ghidra_12.1.2_PUBLIC` ou les
extensions Ghidra sous `ghidra-user/.ghidra/.ghidra_12.1.2_PUBLIC/Extensions/`
sont absents (sandbox réinitialisé), les restaurer avant toute analyse :
`XenonRecomp` depuis le commit épinglé dans
`recompilation/ace-combat-6-retail/config/xbox360-toolchain.lock.json`;
Ghidra 12.1.2 depuis sa release publique officielle
(`NationalSecurityAgency/ghidra`, tag `Ghidra_12.1.2_build`); les extensions
`GhidraXenon`/`XEXLoaderWV` en les copiant depuis les copies déjà trackées
dans ce dépôt (`ghidra-user/.ghidra/.ghidra_12.1.2_PUBLIC/Extensions/`) vers
`<install Ghidra>/Ghidra/Extensions/` — ne jamais installer une version non
épinglée de ces extensions. Invoquer `analyzeHeadless` avec
`JAVA_TOOL_OPTIONS="-Duser.home=$PWD/ghidra-user"` (voir
`analysis/microexec/README.md`).

## Validation minimale

Après un changement natif :

```sh
python3 recompilation/ace-combat-6-retail/tools/build.py \
  --target ntsc-uj --profile native
python3 recompilation/ace-combat-6-retail/tools/validate.py \
  --target ntsc-uj --runtime native
```

Le build courant enregistre CTest 9/9. `release_ready=false` reste attendu.
Ne pas lancer d'oracle, d'A/B ou de trace globale sans ambiguïté causale
nommée. PAL reste bloqué jusqu'au gameplay Mission 01 US visible.

Le catalogue d'architecture local et `docs/native-recompilation-tools.md`
n'existent pas. Utiliser `docs/STATIC_EVIDENCE_TOOLING.md`, le README produit
et les outils déjà présents; ne pas inventer leur contenu manquant.
