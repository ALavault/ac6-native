# Cycle 936 — smoke avec ressource supplémentaire

Le générateur a été exercé avec une troisième ressource NDXR (asset 165,
`sky165`). Le frontend traverse naturellement les états jusqu’à Mission et le
renderer soumet trois géométries (`geometry=3`) sans erreur.

La couverture reste identique car le slice de test réutilise le terrain et ne
constitue pas un sky retail qualifié. Ce run valide la propagation des contrats,
pas la parité visuelle.
