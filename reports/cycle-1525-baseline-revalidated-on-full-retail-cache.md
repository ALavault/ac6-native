# Cycle 1525 — le baseline est revalidé sur le cache retail complet

## Résultat

Le checkpoint 0 est revalidé sur le cache PAL v2 complet, et non sur le chemin
sans ressources qui saute deux tests. Les 81 tests C++ ont tous été exécutés :
le frontend retail et la scène Vulkan Mission 01 passent avec les 926 blobs du
cache. Les 149 tests Python, le lint, les audits de contrats et l'installation
relogeable passent également.

Cette revalidation ne ferme aucune lane du checkpoint 2 : son état reste
`open`, `0/6`. Aucun code produit C++ ni aucune sortie générée n'est modifié.

## Qualification

- Cible : Xbox 360 PAL, module `default.xex`, SHA-256
  `acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde`.
- Projet canonique : `ghidra-projects/ace-combat-6`; aucune écriture Ghidra et
  aucune lecture du projet n'ont été nécessaires pour cette revalidation.
- Cache retail v2 : index
  `cfca517e3f843169ca01fc52700472e66b86365621a922fc27a64a21ab713f85`,
  15 missions, 926 blobs, 5 424 368 676 octets décodés.
- Compilateur : Clang 21.1.8, configuration Release.
- Oracle : aucun lancement Xenia, XenonRecomp ou AC6_recomp.

## Artefact durable corrigé

`reports/ac6-pal-campaign-import-matrix.json` est régénéré depuis le cache
complet. La version précédente décrivait encore le cache frontend intermédiaire
de 24 blobs (`ca25a10f…`) et omettait les quinze entrées monde ; la nouvelle
matrice contient les quinze métadonnées de payload et déclare explicitement
`retail_bytes_embedded=false`. Aucun octet PAC ou payload retail n'est ajouté.

## Validation

```text
cmake --build ... -j16                                  pass
CTest, cache retail + SDL dummy + Xvfb                  81/81, skips 0
  ac6-retail-frontend-resources                         pass, 59.63 s
  ac6-retail-mission01-vulkan-scene                     pass, 106.03 s
tools/tests                                              149/149
ruff tools scripts                                      pass
cache retail v2                                         15 missions, 926 blobs
Mission 01 JF                                           pass
class map J2                                            811 vtables
contract artifacts                                      155/155
contract addresses                                      321/321
contract derivations                                    52, gaps 0
global ladder                                           15 missions, 8 checkpoints
checkpoint 2                                            open, 0/6 lanes
oracle reproducibility                                  pass
camera selector micro-execution                         217 + 32, substitué 0
product boundary                                        239 sources, 1 binaire
installation relogeable                                 85 fichiers, bin/bin absent
git diff --check                                        pass
```

## Frontière suivante

La première fermeture utile reste Scene/TCAM. Le lot suivant doit borner le
chemin caméra `manager+0x4A8 != 0` de `0x82262A28` via `0x82262508`, sans
promouvoir le bloc VMX128 non micro-exécuté ni le producteur live encore
inconnu. Les tests retail dépendants des ressources restent une garde obligatoire.
