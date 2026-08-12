# Cycle 1527 — le normaliseur d'axes mode 3 est borné

## Résultat

Le corps scalaire `0x8225C680` possède maintenant un port C++ manuscrit,
fail-closed, relié à deux contrôles positifs exécutés sur les bytes PAL. Le
rayon, les échelles `1,25/1,0`, le clamp, les zéros signés et l'ordre des deux
stores sont portés. Les résultats trigonométriques arrondis retail restent une
dépendance injectée : aucun `std::sin`, `std::cos` ou `std::atan2` n'est promu
comme équivalent général.

Scene/TCAM reste `open`. Le préfixe VMX/VMX128 de `0x82262508`, les requêtes
tunnel et les producteurs live ne sont pas remplacés par des valeurs
synthétiques dans le produit.

## Qualification et contrôles exécutés

- Xbox 360 PAL, `default.xex`, SHA-256
  `acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde`.
- Projet canonique `ghidra-projects/ace-combat-6`, ouvert avec
  `-readOnly -noanalysis`; `.pdata` `0x8225C680+0xFC`.
- `0x8225C680`, entrée `{1,0}` : 208 étapes, 8 entrées de callees, zéro
  stub/override/sémantique affirmée/pont de registres, sortie
  `3F800000 B33BBD2E`.
- `0x82262738`, direction `(2^-18,2^-19,-2^-18,0)` : les quatre comparaisons
  passent sous le seuil retail `2^-16`, le bloc d'estimation VMX est sauté et
  les deux helpers asin/atan2 retail s'exécutent. Le step-cap exact de 134
  étapes s'arrête avant `0x8226283C`; `f28=2^-19` et `f31=-pi/4`. Aucun stub,
  override, pont ou sémantique affirmée n'est actif.

Le listing corrige une lecture initiale : le premier slot reçoit le produit
sine-like et le second le produit cosine-like. L'API emploie donc des facteurs
nommés par slot, sans réordonner les résultats pour leur donner une convention
cartésienne non prouvée.

## Port et refus

`normalise_mode3_camera_axes` reproduit le `x*x` arrondi avant le `fmaf` de
`y*y`, puis `sqrt`, scale et clamp. Le chemin zéro/zéro conserve séparément les
signes des deux zéros. Tout axe, facteur, intermédiaire ou résultat non fini
échoue fermé. Les tests distinguent le groupement fusionné d'une somme de deux
produits arrondis, couvrent les deux scales, le clamp, les overflows et la
composition avec les coeurs gain/rotation déjà bornés.

## Validation

```text
build complet                                           pass
CTest, cache retail + SDL dummy + Xvfb                  81/81, skips 0
camera selector microexec                               217+32+38+134+208
tests Python                                             164/164
ruff tools scripts                                      pass
Mission 01 JF                                           pass
checkpoint 2                                            open, 0/6 lanes
git diff --check                                        pass
```

## Frontières restantes

Le contrôle sous `2^-16` ne qualifie ni la normalisation VMX avec
`vrsqrtefp/vrefp`, ni les produits matrice/VMX128 en amont. Les facteurs
trigonométriques demeurent injectés jusqu'à qualification complète des helpers.
Le producteur caméra `0x82281198`, ses deux requêtes géométriques, le fallback
vertical, le mode 1 et les joins restants de `0x82262A28` restent ouverts.
