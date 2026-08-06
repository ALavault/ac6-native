# Cycle 896 — seam caméra retail c218–c221

Le produit natif accepte désormais une entrée optionnelle `camera.tsv` dans le
manifeste. Chaque ligne contient `mission_id` puis les 16 coefficients
row-major de la transformation qualifiée :

```text
mission_id c218[0..3] c219[0..3] c220[0..3] c221[0..3]
```

Le renderer applique alors `x*c218 + y*c219 + z*c220 + c221`, division
homogène par `w`, clipping NDC et profondeur `[0,1]`. En l'absence de ce fichier,
le chemin précédent WorldFrame reste inchangé. Le manifeste ne contient encore
aucune valeur inventée.

Cette étape prépare le raccord des matrices relevées dans
`cycle-774-stock-gameplay-terrain-sky-camera.md`; elle ne revendique pas encore
la capture oracle positive ni la parité visuelle.

Validation : build RelWithDebInfo réussi, CTest normal 3/3 et CTest
ASan/UBSan 3/3. Un essai avec les constantes cycle 774 bridge est resté
fail-closed (NDC hors profondeur), ce qui confirme que c218–c221 est une
transformation intermédiaire et non directement une matrice clip complète.
