# AC6 retail NTSC-U/J — reprise Gate 2 runtime natif

Reprendre depuis `recompilation/ace-combat-6-retail`.

Lire d'abord :

- `reports/handoff/CURRENT.json`;
- le report r175 cité comme `source_report`;
- `NEXT.md`;
- `STATE.md` et `EVIDENCE.md` seulement pour une question historique nommée.

## Frontière active

La famille de configuration plateforme Vd/X ouverte par r168 est
entièrement fermée depuis r175 (détail dans `STATE.md` r169-r175). r176 a
corrigé `XamUserGetSigninState` (index 0 → connecté localement); r177 a
corrigé `XamGetSystemVersion` (nécessaire pour que r176 s'applique
réellement à son site d'appel). Candidat identifié, non implémenté :

- `XamInputGetState`/`XamInputSetState`/`XamInputGetCapabilities` : E/S
  manette réelle, candidat le plus prometteur pour le symptôme historique
  « contrôles nuls », mais nécessite un vrai backend d'entrée natif
  (absent de `native/`) — tâche matériellement plus grande qu'un fix de
  forme de contrat.

Sinon, continuer le balayage des imports offline restants (mêmes outils
que r148-r177, méthode r90/r93/r164).

Ne pas supposer qu'un import est un remplissage de structure sans lire ses
sites d'appel réels (r170 a infirmé cette hypothèse pour `VdQueryVideoFlags`),
ni qu'une valeur parmi plusieurs candidates plausibles est arbitraire sans
lire comment CE XEX la consomme (r172, r175).

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
