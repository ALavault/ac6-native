# Le callback n'est pas enregistré — correction de `247eee0b`

Date : 2026-08-18

## Ce que j'ai publié il y a une heure

> « L'enregistrement a lieu ; l'invocation n'a pas lieu, faute d'événement. »

L'enregistrement **n'a pas lieu**. Je l'avais déduit de « `0x821ADC78` est
atteinte, 1 fois », et être atteinte n'est pas faire son travail.

## La mesure

`sub_821ADC78` garde chacun de ses `bctrl` — celui qui déclare la catégorie 2,
celui qui appelle le service 47, celui qui appelle le service 64 — derrière un
test de nullité. Arêtes indirectes enregistrées dans `0x821ADC00..0x821ADF00` :

```text
run START   0 arêtes
run neutre  0 arêtes
```

Aucun de ses appels indirects n'a été exécuté. La fonction entre, échoue à tous
ses tests, et sort.

## L'instrument, mesuré avant d'être cru

Un négatif tiré d'une liste d'arêtes ne vaut que si la liste est complète :

- `record_indirect_edge` insère dans un `std::map` clé `(thread, lr, target)`,
  sans plafond ni éviction ;
- le rapport sérialise tout : `edge_count == len(edges)`, 2 064 pour START et
  2 012 pour le neutre.

## Ce que testent les gardes

```powerpc
r31 = r30 = 0x82000000
r11 = [0x82000610] ; r10 = [r11]      ; si 0 -> branche
r10 = [0x820006E4] ; r10 = [r10]      ; si 0 -> saut par-dessus l'appel
```

Valeurs à l'exécution, tick 222, identiques à celles de l'image au repos :

```text
[0x82000610] = 0x00010266     déréférencé -> 0
[0x820006E4] = 0x00010059     déréférencé -> 0
```

Rien ne les écrit pendant le run.

## Une hypothèse formée puis réfutée

La forme `0x0001xxxx` m'a fait penser à un encodage d'import XEX
`(bibliothèque << 16) | ordinal`. Vérification contre
`analysis/demo/ac6-demo-import-thunks-v1.json` : la table d'imports du jeu est
en `0x82375984`, 228 enregistrements de 16 octets, et **ni l'ordinal 614
(0x266) ni l'ordinal 89 (0x59) n'y figurent**. L'hypothèse est fausse.

Ce que la zone est réellement : une plage **contiguë de 151 mots**
`0x82000560..0x820007BC`, tous de la forme `0x0001xxxx`, bornée par un mot nul
de chaque côté. Sa nature n'est pas établie, et elle n'est pas nommée ici.

## Ce que cela déplace

La chaîne publiée en `247eee0b` reste vraie de bout en bout — un seul écrivain
de `device+0x5460`, non atteint ; un seul lecteur, atteint 11 863 fois — mais
son maillon rompu est **plus haut** que je ne l'ai dit. Ce n'est pas
« `CX360UnitManager` n'est pas construit donc l'événement ne part pas » : le
callback n'est même pas inscrit auprès du service.

## Non établi

- Ce que contient la plage `0x82000560..0x820007BC`, et ce qui devrait y
  écrire des pointeurs.
- Si les deux emplacements sont censés être initialisés par un constructeur
  statique que le port n'exécute pas, ou s'ils ne sont pas des pointeurs et
  ma lecture des gardes est décalée. Les deux restent ouvertes.
