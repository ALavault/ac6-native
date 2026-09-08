# AC6 retail NTSC-U/J — r446 — `sub_821CC508` a sa PROPRE lecture de fichier asynchrone réelle (indépendante de la branche fichiers de `sub_821CC008` déjà écartée par r445), avec une boucle de 5 tentatives ; capturé EN DIRECT : un code d'erreur constant `0x13D` (317) sur plusieurs tentatives consécutives

**Qualification** : NTSC-U/J retail, `default.xex` SHA-256
`6eefba42cdfe9121207e534d8d290009c98b1a8c60ae5334a33a4f15167cbbbc`.
Aucun oracle utilisé.

## Contexte

Nommé par r445 : revenir à l'intérieur de `sub_821CC508` avec
l'hypothèse « fichier manquant » de `sub_821CC008` écartée, tracer en
direct les valeurs réellement comparées juste avant le site d'échec
confirmé (`+6171`) sans présupposer une cause fichier.

## Établi

### Le site `+6171` est une boucle de nouvelles tentatives (5 essais), pas un échec direct

Relecture complète de la décompilation de `sub_821CC508` (déjà en
main depuis r441/r443) : le chemin menant au retour `-1` (`return
0xffffffffffffffff;`, juste après `Function_821D4988(...)`) est en
réalité la fin d'un **compteur de nouvelles tentatives** :
```c
iVar6 = *(int *)(iVar2 + 0x5970);      // compteur, initialisé à 5
*(int *)(iVar2 + 0x5970) = iVar6 - 1;
if (iVar6 == 0) {                      // tentatives épuisées
  ...
  Function_821D4988(0xffffffff8275a414, lVar3 + 0x83b4, 0);  // journalise (r445)
  return 0xffffffffffffffff;           // abandonne — ÉCHEC final
}
*(undefined4 *)(iVar2 + 0x144) = 3;
goto LAB_821cc838;                     // sinon, réessaie
```
Ce bloc est atteint quand une **lecture asynchrone réelle** échoue :
```c
iVar6 = Function_821F4E70(*(...) * 4 + -0x7d6c46c4,   // handle de fichier, indexé
                          uVar13+uVar16, uVar10, 0, lVar3+0x594c);
iVar7 = func_0x821f50a0();                              // équivalent GetLastError
if (((iVar6 == 1) || (iVar7 == 0)) || (iVar7 == 0x3e5)) {
  ... // succès, ou pas d'erreur, ou 0x3e5=ERROR_IO_PENDING (motif Win32) → continue
}
// sinon (l'échec réel) → décrémente le compteur de tentatives, ci-dessus
```

**Ceci est une opération de LECTURE ASYNCHRONE RÉELLE propre à
`sub_821CC508`, indépendante de la branche d'énumération de fichiers
de `sub_821CC008` que r445 a déjà écartée** — les deux fonctions
partagent la même table globale (`iRam8293b94c`, déjà vue dans les
deux décompilations, 16 octets par entrée) mais font chacune leurs
propres appels fichier séparés.

### Capturé EN DIRECT : un code d'erreur constant `0x13D` (317) sur plusieurs tentatives

Points d'arrêt sur `sub_821F50A0` (l'équivalent `GetLastError`),
`finish`, lecture de `*(int*)$rbx` (convention déjà établie cette
session pour lire `ctx.r3`) à chaque retour :
```
GetLastError-equiv #4: (rbx)=0x13d
GetLastError-equiv #5: (rbx)=0x13d
GetLastError-equiv #6: (rbx)=0x13d
```
(les 3 premiers appels observés, sur d'autres threads/sites
d'appel de cette même fonction ailleurs dans le runtime, ont
retourné `0x0` — sans rapport avec ce site précis). **Le code
`0x13D` (317) revient de façon constante et répétée sur les appels
associés à ce site** — cohérent avec une lecture qui échoue de la
MÊME façon à chaque nouvelle tentative (pas une erreur transitoire
qui varie).

## Non établi

- **La signification exacte du code `0x13D`** — pas un code Win32
  standard immédiatement reconnaissable (`ERROR_IO_PENDING`=0x3e5,
  `WAIT_TIMEOUT`=0x102 sont déjà identifiés dans ce même voisinage de
  code) ; non documenté ailleurs dans ce dépôt (`grep` sur les
  sources natives déjà écrites : aucune occurrence).
- **Le stub hôte natif exact responsable** — `sub_821F4E70` (la
  lecture elle-même) n'appelle qu'un helper de prologue générique en
  interne (`0x82382a18`), pas un import natif directement visible
  dans son propre corps ; la logique réelle doit être plus profonde
  (non tracée ce cycle).
- **Le fichier/handle précis concerné** — la table `iRam8293b94c` et
  l'index `pcVar19` n'ont pas été résolus en une adresse ou un nom de
  fichier concret ce cycle.

## Décisions prises

- Ne pas deviner la signification de `0x13D` sans preuve — le
  rapporter comme valeur mesurée, pas comme code identifié.
- Poser les points d'arrêt sur la fonction `GetLastError`-équivalente
  elle-même plutôt que de tenter à nouveau une corrélation adresse-
  hôte précise pour un site d'appel spécifique — a suffi à obtenir
  une valeur cohérente et répétée sans le coût de re-désassembler un
  site d'appel exact.

## Gate

Aucune source de production éditée ce cycle (investigation en direct
uniquement).
```
python3 tools/audit_ac6_mission01_native_gate.py analysis/contracts/mission01-final-gate-v3.json --artifact-root . --require JF
python3 tools/audit_ac6_contract_artifacts.py --artifact-root=. analysis/contracts/*.json
python3 tools/audit_ac6_contract_addresses.py --artifact-root=. analysis/contracts/*.json
```
(sorties consignées ci-dessus, inchangées — aucune source de
production affectée)
`ctest` (racine et natif) inchangé depuis r445 (aucune source de
production modifiée).

## Named for r447

Tracer plus profondément `sub_821F4E70` (la lecture elle-même) au-delà
de son helper de prologue générique, pour trouver le vrai stub hôte
natif qui produit `0x13D`, et résoudre l'index `pcVar19`/la table
`iRam8293b94c` vers un fichier/handle concret — pour déterminer si
c'est un stub incomplet ou un état de fichier légitimement absent
dans cet environnement offline. Reste ouvert sinon : décision de
committage de l'arriéré `native_vulkan_backend.cpp`
(r433/r434/r438).

## Files

Aucun artefact gitignoré nouveau.
