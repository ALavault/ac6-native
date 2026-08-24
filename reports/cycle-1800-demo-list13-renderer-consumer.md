# Cycle 1800 — liste 13 jointe au premier consumer renderer

Le snapshot read-only naturel du tick 3001 ferme la liste 13 : descripteur
`{type=0, offset=0x1998}`, liste `0x2DCB2BB8`, 16 records type 0 portant
`draw_index=0x12`. Le dispatch `0x82325E70`, puis le slot virtuel 7 à
`LR=0x82325ED4`, appellent exactement `0x820EB200` pour chacun des records.

Le build est valide et le run s'arrête comme prévu à `max_ticks=3002`. Les 24
draws typés ne produisent toutefois aucune ressource présentable :
`present_count=0`. Les deux mécanismes de screencap demandés n'ont donc publié
aucune image. La validation visuelle reste ouverte et devient un critère
explicite du prochain gate.

La nouvelle frontière est `0x820EB200` avec `record+0x0C=0x12` : résoudre
l'entrée ABI, son handle de draw et sa queue, puis la joindre au producteur de
sortie présentable. `frontend=false`, `mission=false`, `terminal=false`,
`supported=false`.

