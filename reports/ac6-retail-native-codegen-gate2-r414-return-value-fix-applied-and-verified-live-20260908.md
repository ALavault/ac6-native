# AC6 retail NTSC-U/J — r414 — correctif appliqué et vérifié en direct : `sub_821FA9E0` retourne désormais le bon pointeur, le tableau croissant statique C++ grandit normalement (883 → 67239 événements alloc/free sur le même run)

**Qualification** : NTSC-U/J retail, `default.xex` SHA-256
`6eefba42cdfe9121207e534d8d290009c98b1a8c60ae5334a33a4f15167cbbbc`.
Aucun oracle utilisé.

## Contexte

Suite de r413 (nommé pour r414) : appliquer et vérifier en direct un
correctif qui sauvegarde `ctx.r3` avant le bloc `if
((r23.u32 & 1u) != 0)` du correctif r366 et le restaure juste après
`RtlLeaveCriticalSection`, confirmer par capture live que
`sub_821FA9E0` retourne désormais le bon pointeur, que `sub_8237FA50`
commet effectivement `begin`, et mesurer si le débordement
r410/r411/r412 disparaît sur un run complet.

## Correctif appliqué

Nouveau script, `recompilation/ace-combat-6-retail/tools/
apply_sub_821fa9e0_leave_return_fix.py` (source de production, sous
contrôle de version — ne modifie PAS `apply_sub_821fa9e0_leave_fix.py`
de r366, patche le bloc que CE script insère déjà) :

```c
if ((r23.u32 & 1u) != 0) {
    uint64_t r414_saved_r3 = ctx.r3.u64;
    ctx.r3.u64 = PPC_LOAD_U32(r27.u32 + 1408);
    __imp__RtlLeaveCriticalSection(ctx, base);
    ctx.r3.u64 = r414_saved_r3;
    r23.u64 = r23.u64 ^ 1;
}
```

Applique une sauvegarde/restauration de `ctx.r3` strictement autour de
l'appel `RtlLeaveCriticalSection` — l'appel lui-même (le vrai
correctif de r366 pour la fuite de section critique) n'est pas
touché. Appliqué sur l'arbre généré actuel
(`build/ntsc-uj/native/codegen-20260831-mapfix-96838/generated/
ppc_recomp.27.cpp`, qui portait déjà le marqueur `BUGFIX (r366,
INVENTED)` — le script exige sa présence et échoue sinon) puis
`ac6recomp` reconstruit (`ninja ac6recomp`, succès, aucune erreur de
compilation).

## Établi

### 1. Le même harnais de capture (r424) confirme le correctif, aux trois étages

Rejoué SANS MODIFICATION (`r424_return_chain.gdb`) sur le binaire
reconstruit :

| étage | avant (r413) | après (ce cycle) |
|---|---|---|
| retour de `sub_821F9E10` | `0x100015a0` | `0x100015a0` (inchangé, jamais le problème) |
| retour de `sub_821FA9E0` | `0x0` | **`0x100015a0`** |
| retour de `sub_823857E0` | `0x0` | **`0x100015a0`** |
| `TEST-SITE` dans `sub_8237FA50` | `eax=0x0` | **`eax=0x100015a0`** |

Les trois occurrences capturées dans ce run (`realloc=3 grow=3 test=3`,
contre `1` chacune avant le correctif) montrent le pointeur correct
propagé de bout en bout à chaque fois, pas seulement pour la première
croissance.

### 2. Le tableau croît désormais normalement — signal indirect mais très fort

Le compteur combiné alloc/free (`total-seq`, le même compteur utilisé
et croisé par r406-r413) passe de **`883`** (avant, tout le run) à
**`67239`** sur le MÊME point d'arrêt de fin de capture (`AC6_NATIVE_
PROBE_WINDOW_MS=45000`, même configuration). Le tableau croissant du
C++ statique effectue désormais de VRAIES croissances répétées
(doublement de capacité à chaque saturation, comme prévu par
`sub_8237FA50`) au lieu de déborder silencieusement dans la mémoire
adjacente sans jamais retoucher l'allocateur (r412) — cohérent avec un
grand nombre de constructeurs statiques enregistrant chacun un
pointeur, chaque saturation de capacité déclenchant maintenant un
véritable cycle alloc+copie+libération au lieu d'être masquée par la
sentinelle `-1` (r412) qui ne se déclenche plus puisque `begin` est
désormais correctement mis à jour vers un tampon toujours VIVANT.

### 3. Le processus reste stable

`gdb-stdout` du run de vérification : `probe window elapsed`,
`presented_frames=0 state=1` (état connu, sans rapport avec ce
correctif — documenté ailleurs dans le projet), `Inferior 1 ... exited
normally`. Aucun crash, aucun blocage introduit par le correctif.

## Ce que ceci établit pour r399-r413

**La chaîne causale complète r399→r413 est désormais vérifiée par un
correctif appliqué et confirmé en direct, pas seulement expliquée.**
Le débordement de tas non borné documenté par r410/r411/r412, dont la
cause a été tracée par r413 jusqu'à une régression précise dans le
correctif r366, ne se produit plus sous ce harnais : `begin` est
maintenant mis à jour à chaque croissance réussie (déduit du retour
correct de `sub_821FA9E0`/`sub_823857E0`/`sub_8237FA50`, la sentinelle
`-1` de r412 ne pouvant plus s'appliquer à un pointeur `begin`
toujours vivant), et le nombre d'événements alloc/free explose dans le
sens attendu d'un tas fonctionnant normalement.

## Non établi

- **Si ceci ferme réellement le blocage r399 original** (le
  double-octroi bucket-17 documenté par r401-r409 à `seq=874`/`877`
  dans l'ANCIEN comportement) — le compteur de séquence n'est plus
  directement comparable (`67239` événements contre `883`), et aucune
  capture de ce cycle n'a spécifiquement recherché un double-octroi
  équivalent dans le nouveau run. Nommé pour le cycle suivant.
- **Si `presented_frames=0` (visible dans les deux runs, avant et
  après) a une cause commune avec la chaîne r399-r413** ou est un
  problème totalement indépendant, déjà documenté séparément dans le
  projet — non examiné ce cycle.
- **Couverture des autres appelants potentiels de `sub_821FA9E0`** au
  delà de `sub_823857E0` — toujours non recensée (héritée de r413).
- Aucune capture-écran ni comparaison de rendu n'a été effectuée —
  seul le comportement de l'allocateur a été vérifié.

## Décisions prises

- Écrire un NOUVEAU script de correctif (`apply_sub_821fa9e0_leave_
  return_fix.py`) plutôt que de modifier `apply_sub_821fa9e0_leave_
  fix.py` de r366 — préserve l'historique et la discipline du projet
  (une source, un script, par défaut ; le script de r366 documente
  correctement SA PROPRE justification et reste correct pour la fuite
  qu'il corrige, seul l'oubli de sauvegarde est en cause).
- Rejouer le harnais r424 SANS MODIFICATION plutôt que d'écrire une
  nouvelle capture, pour une comparaison avant/après strictement
  contrôlée.

## Gate

**Source de production éditée ce cycle** :
`recompilation/ace-combat-6-retail/tools/apply_sub_821fa9e0_leave_return_fix.py`
(nouveau fichier, sous contrôle de version).
```
python3 tools/audit_ac6_mission01_native_gate.py analysis/contracts/mission01-final-gate-v3.json --artifact-root . --require JF
python3 tools/audit_ac6_contract_artifacts.py --artifact-root=. analysis/contracts/*.json
python3 tools/audit_ac6_contract_addresses.py --artifact-root=. analysis/contracts/*.json
```
(sorties consignées dans le commit de ce cycle)
`ctest` relancé en entier.

## Named for r415

Rechercher, sur le run corrigé, si un double-octroi équivalent à
`seq≈874`/`877` (r401-r409) se produit encore ailleurs dans la
séquence désormais bien plus longue (`67239` événements) — ou si le
correctif de ce cycle ferme réellement le blocage r399 original.

## Files

`recompilation/ace-combat-6-retail/tools/apply_sub_821fa9e0_leave_return_fix.py`,
`recompilation/ace-combat-6-retail/artifacts/retail-us-native-r404-bucket17-writer/r424_return_chain.gdb/.log`
(rejoué, log écrasé par ce cycle — capture avant/après documentée dans
ce rapport, pas dans deux fichiers séparés).
