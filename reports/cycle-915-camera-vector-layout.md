# Cycle 915 — layout explicite des constantes c218–c221

Date : 2026-08-04

## Contrat fermé

Les observations runtime qualifiées décrivent le VS comme :

```text
position.x*c218 + position.y*c219 + position.z*c220 + c221
```

Chaque constante est donc un vecteur de sortie. Le loader caméra conserve la
compatibilité avec les manifests historiques (16 coefficients interprétés en
lignes), et accepte désormais un dix-septième champ `column_major` pour
représenter l’opération retail sans ambiguïté. Le renderer sélectionne alors
les coefficients transposés au moment de la projection homogène.

## Validation

- rebuild RelWithDebInfo : succès ;
- manifeste terrain temporaire avec `column_major` chargé par
  `--present-manifest` : readback couleur/profondeur écrit ;
- un manifeste historique sans suffixe reste accepté ;
- aucune valeur caméra n’est générée par le runtime.

La caméra terrain issue du draw 259 ne constitue toujours pas une preuve de
parité : ses transforms de scène et le raccord oracle 1800 ticks restent à
fermer.
