# Cycle 1612 — provenance des stores VMX et frontière PM4 bootstrap

## Résultat

L'instrumentation opt-in `AC6_DEMO_WATCH_IB_WRITERS=1` couvre désormais les
stores PPC de 1, 2, 4, 8 et 16 octets. Le générateur XenonRecomp épinglé
n'émet plus les `stvx/stvx128` comme stores hôte directs : il produit
`PPC_STORE_U128`, qui conserve l'inversion des 16 bytes Xenon big-endian et
passe par `GuestMemory`. Les protections, générations de write et erreurs de
mapping s'appliquent donc aussi aux stores VMX. Aucun C++ généré n'est édité ou
suivi ; seule la transformation reproductible est portée par le patch du
générateur.

Le codegen strict reste à 12 867 fonctions, 145 records et 52 unités, avec
zéro diagnostic de frontière et zéro instruction non supportée. CTest codegen
ON passe 13/13, y compris audits source et complexité.

## Observation dynamique

Avec le processeur PM4 fail-closed actuel, l'exécution s'arrête au tick 0 avant
la production de l'IB principal historique `0x1274A000`. Les IB effectivement
capturés avant l'arrêt sont :

- `0x1686A040`, 11 dwords ;
- `0x1685A000`, 64 dwords ;
- `0x16ADF000`, 74 dwords ;
- `0x16ADFD40`, 13 dwords ;
- `0x16AE0980`, 48 dwords.

Leur première et dernière génération de write valent zéro dans cette
exécution. Cette observation ne qualifie pas leur producteur : elle prouve que
les bytes ont été déposés avant le nouveau pont de store observable, ou par
une voie qui doit encore être identifiée. Elle invalide en revanche
l'hypothèse selon laquelle le run fail-closed courant atteint déjà les writers
de `[0x1274A000,0x1274CF54)`.

## Limite et prochain test

Le rapport historique du tick 253 reste la preuve byte-exacte de l'IB
principal, mais ne contient aucun PC de writer. Rejouer au-delà du registre
inconnu `0x0A02` en l'ignorant constituerait un effet approximatif interdit.
La provenance du main IB demeure donc ouverte jusqu'à qualification de ce
registre. Le prochain checkpoint cherche une autorité Xenos primaire pour
`0x0A02..0x0A05`; à défaut, il remonte le producteur statique du packet et
compare son ABI/effet à une capture démo ciblée, sans promouvoir de nom retail.
