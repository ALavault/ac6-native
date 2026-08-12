# Cycle 1524 — la cible caméra directe est exécutée

## Résultat

Le chemin scalaire direct du sélecteur caméra mode 2 est désormais natif et
fail-closed. Il ne remplace pas encore tout `0x82262A28` : le chemin
`manager+0x4A8 != 0`, les corrections propres aux modes 1/3, le producteur de
l'objet passé en `r5` et la requête globale de suppression restent ouverts.

La courbe cubique `0x8225D660`, appelée par le chemin mode 3, est également
portée. Aucun statut Scene/TCAM ni JV ne passe avec ce lot.

## Qualification et preuve statique

- Cible `Xbox 360 PAL`, module `default.xex`, SHA-256
  `acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde`.
- Projet Ghidra canonique `ghidra-projects/ace-combat-6`, ouvert avec
  `-readOnly -noanalysis`.
- Frontière canonique `0x82262A28`; appel direct unique à `0x82263C1C` dans
  `0x82263A50`.
- Le manager entrant est conservé dans `r31` à `0x82262A3C`. L'objet cible
  entrant est conservé dans `r30` par `0x82263A50` puis transmis en `r5` à
  `0x82263C10`. Le seul appelant direct non nul observé est le chemin
  `0x8226DDE0`, qui retransmet lui-même son `r5`; son producteur nominal reste
  donc une frontière explicite.

Le chemin direct qualifié est :

```text
0x82262A4C..0x82262A6C  axes initialisés à zéro
0x82262A70..0x82262AB4  gardes +0x4A0/+0x4A8, lectures r5+0xE88/E8C
0x82262AE4..0x82262B08  gain +0x364 et choix +0x360/+0x350
0x82262D60..0x82262DA0  clamp des deux axes à [-1,+1]
0x82262DA0..0x82262E28  identité et requête de suppression
0x82262E68..0x82262F10  consommation des cibles et écriture +0x3A0/3A4/3A8
```

Les bornes sont les mots retail `0xBF800000` à `0x82069B28` et
`0x3F800000` à `0x82001348`. Le mode 2 direct choisit `manager+0x360`
uniquement pour un premier axe strictement positif ; zéro et négatif prennent
`manager+0x350`. `manager+0x364` multiplie l'axe issu de `-target+0xE8C`.

`0x8225D660` borne son paramètre à `[0,1]`, puis évalue les quatre floats à
`manager+0x350` dans la forme cubique de Bernstein. Les trois accumulations
finales sont des `fmadds`; le port conserve leurs arrondis avec `std::fmaf`.

## Contrôles exécutés

`MicroExecuteFunction.java` a exécuté les instructions retail dans le projet
canonique, sans sémantique assertée, sans pont de registres vectoriels et sans
stub :

```text
0x82262A28  camera-mode2-direct-selector       217 instructions
             +0x3A0/+0x3A4/+0x3A8 = 3E800000/BD000000/3E800000
0x8225D660  camera-mode3-gain-curve-quarter     32 instructions
             f1 = 1.953125 (float 3FFA0000)
```

`tools/audit_ac6_camera_selector_microexec.py` verrouille l'identité du XEX,
les deux sorties, les nombres d'instructions et l'absence de sémantique
substituée. Son test négatif injecte une sémantique assertée et exige le refus.

Les tests C++ couvrent le contrôle positif bit pour bit, les axes positifs et
négatifs, les deux gardes, le clamp, la suppression injectée, les paramètres
hors `[0,1]`, les non-finis et la composition avec le cœur de rotation déjà
qualifié. Le cache persistant de projet
`.tools/ac6-native-cache-cp3-20260811`, index
`cfca517e3f843169ca01fc52700472e66b86365621a922fc27a64a21ab713f85`,
résout encore les quinze groupes caméra retail.

## Validation

```text
build Clang 21.1.8                                      pass
CTest, cache retail + SDL dummy + Xvfb                  81/81
tools/tests                                              149/149
ruff tools scripts                                      pass
camera selector microexec                               217 + 32, substitué 0
cache retail, groupes caméra mode 2                     15/15
global ladder                                           15 missions, 8 checkpoints
checkpoint 2                                            open, 0/6 lanes
Mission 01 JF                                           pass
contract artifacts                                      31/31
contract addresses                                      103/103
contract derivations                                    18, gaps 0
product boundary                                        239 sources, 1 binaire
git diff --check                                        pass
```

## Frontières restantes

Le contrat Scene/TCAM reste ouvert. Il faut encore fermer `0x82262508` pour le
chemin `+0x4A8`, la normalisation `0x8225C680`, le correctif de cap mode 1,
l'override mode 1 état 7, la production de la requête `0x82281198`, puis le
producteur live des locators joueur/manager et le recensement TCAM des quinze
payloads. Le bloc VMX128 mode 1 est décodé statiquement mais n'a pas de contrôle
micro-exécuté complet dans le langage Ghidra actuel ; aucune sémantique native
n'en est déduite ici.
