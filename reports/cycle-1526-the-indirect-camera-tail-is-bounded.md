# Cycle 1526 — la queue caméra indirecte est bornée

## Résultat

Le chemin mode 2 `manager+0x4A8 != 0` possède maintenant sa queue scalaire
native, fail-closed et micro-exécutée. Le port commence à `0x8226283C`, après
le producteur vectoriel de `0x82262508`, puis compose ses deux axes normalisés
avec le sélecteur `0x82262A28` et le cœur de rotation déjà qualifié.

Ce lot ne ferme pas Scene/TCAM. Le bloc VMX/VMX128
`0x82262598..0x82262838`, qui construit les axes depuis les objets live, reste
hors du port ; aucune valeur synthétique ne le remplace dans le produit.

## Qualification et preuve statique

- Cible Xbox 360 PAL, module `default.xex`, SHA-256
  `acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde`.
- Projet Ghidra canonique `ghidra-projects/ace-combat-6`, ouvert avec
  `-readOnly -noanalysis` ; fonction `.pdata` `0x82262508+0x520`.
- Appel direct unique à `0x82262508` : `0x82262A94`, derrière les gardes
  `manager+0x4A0` et `manager+0x4A8` de `0x82262A28`.
- Le checkout propre AC6_recomp `dcd41b7457fcac8242f8ef40de83d1719390d5af`
  a servi uniquement de cross-match littéral des mnémoniques VMX128 que Ghidra
  n'affiche pas ; aucun code généré ni nom généré n'entre dans le port.

Avec le drapeau mode 3 de `r27` nul, la queue qualifiée est :

```text
0x8226283C..0x82262874  borne f31 par manager+0x364
0x82262878..0x822628B4  divise et borne f31 à [-1,+1]
0x822628B4..0x822628E8  choisit +0x360 si f28>0, sinon +0x350, puis borne f28
0x822628E8..0x82262930  traite la limite zéro, divise, borne et écrit les axes
```

Les constantes sont les mots retail `0x00000000` à `0x8200082C`,
`0xBF800000` à `0x82069B28` et `0x3F800000` à `0x82001348`.
Une limite négative ou un flottant non fini est hors du domaine qualifié et
échoue fermé côté natif.

## Contrôle exécuté et port natif

`MicroExecuteFunction.java` démarre directement à `0x8226283C` et intercepte
seulement l'épilogue `0x82262A10`. Avec `f28=2`, `f31=-1`,
`manager+0x360=4` et `manager+0x364=2`, les 38 instructions retail écrivent :

```text
premier axe  0x3F000000  (+0,5)
second axe   0xBF000000  (-0,5)
```

Le snapshot refuse toute sémantique substituée, tout pont de registres
vectoriels et toute sortie dépendante du poison mémoire. L'auditeur possède un
contrôle négatif sur la provenance et un autre sur les octets de sortie.

`normalise_mode2_indirect_camera_axes` porte cette queue scalaire.
`select_mode2_indirect_camera_rotation` réapplique les gains dans l'ordre de
`0x82262AE4..0x82262E64`, conserve la suppression injectée comme frontière du
producteur `0x82281198`, demande le wrap de `+0x3A4`, puis alimente
`step_mode2_camera_rotation`. Les tests couvrent les deux signes, les limites
zéro, le clamp, la suppression, les non-finis et la composition bit à bit.

## Validation

```text
build Clang 21.1.8                                      pass
CTest, cache retail + SDL dummy + Xvfb                  81/81, skips 0
tools/tests                                              150/150
ruff tools scripts                                      pass
camera selector microexec                               217 + 32 + 38
cache retail, groupes caméra mode 2                     15/15
Mission 01 JF                                           pass
contract artifacts                                      156/156
contract addresses                                      321/321
contract derivations                                    52, gaps 0
product boundary                                        239 sources, 1 binaire
installation relogeable                                 85 fichiers, bin/bin absent
checkpoint 2                                            open, 0/6 lanes
git diff --check                                        pass
```

## Frontières restantes

Le préfixe vectoriel de `0x82262508`, la branche mode 3
`0x82262934..0x82262A0C`, `0x8225C680`, le correctif mode 1, le producteur de
la requête `0x82281198`, le locator joueur live et le recensement Scene/TCAM
des quinze payloads restent ouverts. Aucun statut JV n'est modifié.
