# AC6 retail NTSC-U/J — r428 — les 7 arguments de `VdSwap` capturés en direct : `r6` pointe vers la zone `readback` déjà connue, `r5` est un bloc de format/mode statique inchangé d'un appel à l'autre, `r4` est un état par-appel qui porte probablement l'adresse du tampon frontal — décodage partiel, pas encore complet

**Qualification** : NTSC-U/J retail, `default.xex` SHA-256
`6eefba42cdfe9121207e534d8d290009c98b1a8c60ae5334a33a4f15167cbbbc`.
Aucun oracle utilisé.

## Contexte

Suite de r427 (nommé pour r428) : décortiquer la sémantique exacte des
sept arguments de `VdSwap`, puis concevoir/appliquer/vérifier en
direct un correctif qui synthétise un vrai paquet PM4 `XE_SWAP`.

## Établi

### Chemin secondaire écarté au passage : le bloc juste avant l'appel `VdSwap` n'est PAS le paquet manquant

Avant de capturer les arguments en direct, le bloc de code
immédiatement précédent (`sub_821F03B0`, autour de `loc_821F0530`)
écrit bien QUATRE mots dans l'anneau — mais décodage de ses
constantes (`lis`/`ori`) donne `0xC0025800` (type 3, compte 3,
opcode `0x58`), `0x80000003`, un mot calculé, et `0xDEADBEEF` : un
paquet `EventWriteShd` (`kOpcodeEventWriteShd=0x58`, déjà nommé dans
`native_xenos.h`) tout à fait ordinaire — `0xDEADBEEF` est une valeur
de charge utile codée en dur (fanion/sentinelle), pas un artefact de
pile non initialisée. Ce n'est PAS le paquet `XE_SWAP` manquant,
cohérent avec les décomptes `event_write` déjà vus dans toutes les
traces précédentes.

### Les 7 arguments, capturés en direct sur les 5 appels de la fenêtre de sonde

`r428_vdswap_args.gdb` (lecture `ctx.rN` par foulée de 8 octets à
partir de `ctx+0x00=r3`, cohérente avec la convention déjà validée
pour cette fonction par r426) :

```
VdSwap #1 r3=0x125c068c r4=0x8feff8c0 r5=0x8feff940 r6=0x16520008 r7=0x8feff990 r8=0x00000000 r9=0x8feff924 r10=0x8feff920
```
(motif identique aux 5 appels pour `r6` et pour le contenu pointé par
`r5`, `r3`/`r4`/`r7`/`r9`/`r10` variant d'un appel à l'autre — voir le
fichier journal complet pour les 5 jeux de valeurs.)

- **`r6 = 0x16520008`, fixe sur les 5 appels.** Cette adresse est dans
  la MÊME zone que `readback` (`0x1652003c`, déjà nommée par r418) et
  que les adresses d'écriture d'événement déjà tracées
  (`0x16520000`/`0x16520004`) — une région de notification/MMIO
  invitée déjà connue de cette investigation, pas une nouvelle
  inconnue.
- **`r5` pointe vers un bloc de 8 mots qui NE CHANGE JAMAIS d'un appel
  à l'autre** :
  `0x8a000002 0x135c0006 0x0059e4ff 0x00001414 0x00000000 0x00000200 0x00000000 0x00000004`.
  Un bloc statique et invariant est cohérent avec un descripteur de
  format/mode d'affichage fixé une fois pour toute la session (pas une
  charge utile par image) — non décodé champ par champ.
- **`r4` pointe vers un bloc de 8 mots qui VARIE à chaque appel** —
  le mot 3 (offset `+12`) correspond systématiquement à une adresse
  proche du curseur d'anneau de l'appel précédent (ex. `0x125c056c`
  à l'appel n°1, à comparer au curseur `0x125c068c` — `0x120` octets
  d'écart) ; le mot 5 (offset `+20`) prend des valeurs plausibles
  d'adresse mémoire GPU/texture (`0x2e33449c`, `0x8288db80`) —
  **candidat le plus probable pour l'adresse du tampon frontal, mais
  non confirmé**. Le mot 1 (offset `+4`) varie de façon irrégulière
  (`0xffffffff`, `0`, `0x500`, `0x12`, `0x500`) — pourrait être un
  fanion ou un compteur, non identifié.

## Non établi

- **La correspondance exacte champ-par-champ avec la signature réelle
  de `VdSwap`** (quel argument est le pointeur de tampon frontal, quel
  argument porte les dimensions) — ce cycle identifie des CANDIDATS
  plausibles pour certains champs (mot 5 de `r4` pour le tampon
  frontal) mais ne les confirme pas par recoupement indépendant (par
  exemple, vérifier que cette adresse correspond à une région
  mémoire effectivement allouée comme surface de rendu).
- **La sémantique de `r7`/`r8`/`r9`/`r10`** — capturés mais non
  analysés ce cycle.
- **Aucun correctif de synthèse PM4 n'a été conçu ni appliqué** — le
  décodage n'est pas encore assez sûr pour écrire un paquet `XE_SWAP`
  correct sans risquer d'inventer une valeur non vérifiée (précédent
  r1111/r1113 : ne pas deviner sans preuve).

## Décisions prises

- Vérifier d'abord si le bloc de code juste avant l'appel `VdSwap`
  était le paquet manquant (décodage de ses constantes) avant de
  capturer les arguments — a permis d'écarter cette piste rapidement
  et à bon compte (paquet `EventWriteShd` ordinaire, déjà comptabilisé
  ailleurs).
- Ne pas deviner la sémantique complète des 7 arguments à partir d'un
  seul jeu de captures — publier ce qui est confirmé (motifs
  invariants vs variants, adresses reconnues) et nommer explicitement
  ce qui reste incertain plutôt que de conclure prématurément
  (précédent CLAUDE.md : ne pas asserter une valeur non vérifiée).

## Gate

Aucune source de production éditée ce cycle.
```
python3 tools/audit_ac6_mission01_native_gate.py analysis/contracts/mission01-final-gate-v3.json --artifact-root . --require JF
python3 tools/audit_ac6_contract_artifacts.py --artifact-root=. analysis/contracts/*.json
python3 tools/audit_ac6_contract_addresses.py --artifact-root=. analysis/contracts/*.json
```
(sorties consignées dans le commit de ce cycle)
`ctest` relancé en entier.

## Named for r429

Confirmer si le mot `+20` de `r4` (candidat tampon frontal) pointe
bien vers une région mémoire cohérente avec une surface de rendu
(taille, alignement, contenu plausible de pixels/texture) ; décoder
`r7`/`r8`/`r9`/`r10`. Une fois la sémantique confirmée avec une
certitude suffisante, concevoir et appliquer le correctif du stub
`VdSwap` qui synthétise un paquet PM4 `XE_SWAP` valide.

## Files

`recompilation/ace-combat-6-retail/artifacts/retail-us-native-r404-bucket17-writer/r428_vdswap_args.gdb/.log`.
