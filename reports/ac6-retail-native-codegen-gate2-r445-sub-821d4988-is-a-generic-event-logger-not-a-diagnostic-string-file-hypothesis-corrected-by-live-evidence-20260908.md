# AC6 retail NTSC-U/J — r445 — `sub_821D4988` décompilée : un journal d'événements générique à tampon circulaire, PAS une chaîne de diagnostic (explique l'échec de lecture de r444) ; l'hypothèse « fichier manquant » de r443/r444 est CORRIGÉE par la preuve en direct — `sub_821CC008` n'emprunte même pas sa branche d'énumération de fichiers sur ce run

**Qualification** : NTSC-U/J retail, `default.xex` SHA-256
`6eefba42cdfe9121207e534d8d290009c98b1a8c60ae5334a33a4f15167cbbbc`.
Aucun oracle utilisé.

## Contexte

Nommé par r444, deux pistes : (1) décompiler `sub_821D4988` pour
comprendre son rôle et l'échec de lecture de chaîne ; (2) tracer en
direct `sub_821CC008` pour identifier le fichier/chemin impliqué.

## Outil ajouté

`scripts/DecompileD4988Logger.java` — même méthode que les scripts
précédents, verrouillé au bon SHA. Committé.

## Établi

### `sub_821D4988` est un journal d'événements générique à tampon circulaire, PAS un afficheur de chaîne

```c
void Function_821D4988(undefined8 param_1, undefined4 param_2, undefined4 param_3) {
  func_0x823d007c(0xffffffff829e637c);          // verrou (section critique)
  *(ulonglong*)(uRam829e6374*8 + BASE) = CONCAT44(param_2, param_3);  // écrit dans un tampon circulaire de 64 entrées
  iRam829e6378 = iRam829e6378 + 1;
  uRam829e6374 = (uRam829e6374 + 1) & 0x3f;      // index modulo 64
  func_0x823d008c(0xffffffff829e637c);           // déverrouille
  iVar1 = func_0x821f5988(4, 0xffffffff829e63a8, 0, 0);  // attente/synchronisation
  if (iVar1 != 0x102) {                          // 0x102 = 258 = WAIT_TIMEOUT (motif Win32)
    Function_821F4210(...);
    Function_821F4130(...);
  }
}
```

**Ceci explique précisément pourquoi la lecture de r444 à l'adresse
`0x8275a414` a retourné une chaîne vide** : ce n'est pas une chaîne
du tout — `param_2`/`param_3` (les deux moitiés 32 bits de l'argument
combiné `ctx.r3`, `0x8275a414`, en réalité passé comme une VALEUR
NUMÉRIQUE, pas un pointeur) sont écrites telles quelles dans un
tampon circulaire d'événements — c'est un mécanisme de journalisation/
télémétrie interne (verrou + écriture + attente courte avec
`WAIT_TIMEOUT` comme issue normale), pas une fonction d'affichage de
message.

### Correction en direct : `sub_821CC008` n'emprunte PAS sa branche d'énumération de fichiers sur ce run

r441 avait décompilé `sub_821CC008` comme ayant deux branches
structurelles distinctes selon `cRam8293b938` : une qui énumère de
vrais fichiers par chemin (ouverture/taille/fermeture, format
`"game:\\%s"` confirmé en lisant la chaîne réelle en direct à
`0x82067934`), une autre plus simple (arithmétique sur des structures
déjà en mémoire, aucun accès fichier).

Points d'arrêt posés sur les instructions EXACTES de la branche
« fichiers » (construction du chemin à `+1187`, appel d'ouverture à
`+1201`, adresses hôte confirmées par désassemblage en direct) :
**aucun des deux ne se déclenche.** Le processus enchaîne directement
de l'entrée de `sub_821CC008` au crash déjà connu dans
`sub_821D6C20`, sans jamais toucher le code d'ouverture de fichier.

**Ceci corrige l'hypothèse de travail de r443/r444** (« une opération
de fichier/contenu échoue ») : sur ce run, `sub_821CC008` prend la
branche SANS accès fichier — la véritable cause de l'échec de
`sub_821CC508` (retour `-1`, r443/r444) n'est PAS liée à
l'énumération de fichiers de `sub_821CC008` sur ce chemin d'exécution
précis.

## Non établi

- **Ce que représente `cRam8293b938`** et pourquoi il vaut ce qu'il
  vaut dans cet environnement (déterminant quelle branche de
  `sub_821CC008` est prise) — non tracé ce cycle.
- **La cause réelle de l'échec de `sub_821CC508`**, maintenant que
  l'hypothèse « fichier manquant » est écartée pour ce chemin — reste
  ouverte, nécessite de revenir à l'intérieur de `sub_821CC508`
  elle-même (déjà partiellement décompilée par r443/r444) avec cette
  nouvelle information.

## Décisions prises

- Corriger explicitement l'hypothèse de travail plutôt que de la
  laisser implicite ou de forcer une conclusion — la preuve en direct
  contredit directement ce qui avait été supposé plausible par r443/
  r444 ; le rapporter est plus utile que de le taire.
- Committer l'outil (`scripts/DecompileD4988Logger.java`) — utile
  indépendamment, a permis de clore définitivement la question de la
  chaîne de r444.

## Gate

**Source committée ce cycle** : `scripts/DecompileD4988Logger.java`
(nouvel outil, aucune source de production touchée).
```
python3 tools/audit_ac6_mission01_native_gate.py analysis/contracts/mission01-final-gate-v3.json --artifact-root . --require JF
python3 tools/audit_ac6_contract_artifacts.py --artifact-root=. analysis/contracts/*.json
python3 tools/audit_ac6_contract_addresses.py --artifact-root=. analysis/contracts/*.json
```
(sorties consignées ci-dessus, inchangées — aucune source de
production affectée)
`ctest` (racine et natif) inchangé depuis r444 (aucune source de
production modifiée).

## Named for r446

Revenir à l'intérieur de `sub_821CC508` (déjà décompilée par r443,
déjà désassemblée dans le projet Ghidra `ac6-us`) avec l'information
que la branche fichiers de `sub_821CC008` n'est PAS en cause — tracer
en direct les valeurs réellement lues/comparées juste avant le site
d'échec confirmé (`+6171`, r444) pour comprendre la vraie condition
déclenchante, sans présupposer une cause fichier. Reste ouvert
sinon : décision de committage de l'arriéré
`native_vulkan_backend.cpp` (r433/r434/r438).

## Files

Nouveau : `scripts/DecompileD4988Logger.java` (committé). Aucun
artefact gitignoré nouveau sous `reports/`.
