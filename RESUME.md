# AC6 retail NTSC-U/J — reprise Gate 2 runtime natif

Reprendre depuis `recompilation/ace-combat-6-retail`.

Lire d'abord :

- `reports/handoff/CURRENT.json`;
- le report r168 cité comme `source_report`;
- `NEXT.md`;
- `STATE.md` et `EVIDENCE.md` seulement pour une question historique nommée.

## Frontière active

`VdQueryVideoMode` est fixé (r169) : offsets `+0x00`/`+0x04`/`+0x08`/`+0x14`
dérivés des deux sites d'appel réels de ce XEX, remplissage depuis l'état Vd
natif. `+0x0C`/`+0x10` restent non lus et non implémentés.

Les trois imports Vd voisins — `VdQueryVideoFlags`, `VdGetCurrentDisplayGamma`,
`VdGetCurrentDisplayInformation` — ne sont pas encore vérifiés pour le même
type de trou. Commencer statiquement dans `ghidra-projects/ac6-us.gpr` /
programme `default.xex` pour chacun : fermer les xrefs directs et indirects,
déterminer si c'est un remplissage de structure ou une simple forme de
contrat, typer les offsets lus, puis seulement implémenter le remplissage
depuis l'état Vd natif existant. Ne pas copier un layout depuis une
réimplémentation indépendante.

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
