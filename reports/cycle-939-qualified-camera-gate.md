# Cycle 939 — caméra explicitement qualifiée

Les lignes caméra peuvent désormais porter `qualified` (avec
`column_major` facultatif). Le chemin `--compare-mission01` refuse toute caméra
qui n’a pas ce marqueur ; `--present-manifest` conserve l’usage développeur des
caméras non qualifiées.

Le test de parsing couvre les combinaisons qualifiée/column-major et les tokens
inconnus. Cette gate empêche qu’une matrice de diagnostic soit présentée comme
référence retail.
