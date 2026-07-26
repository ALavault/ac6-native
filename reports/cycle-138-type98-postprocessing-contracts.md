# AC6 — contrats statiques du post-traitement type `0x98`

Date : 2026-07-17 (Europe/Paris)

## Cible et méthode

Cible : `default.xex` Xbox 360 PAL, SHA-256
`acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde`.

Cette passe est une lecture headless du projet Ghidra corrigé. Aucun runtime,
émulateur, GUI ou fichier généré n'a été modifié.

## Résultat

Les appels qui suivent la réinjection du résultat `owner+0x15c` sont maintenant
classés par contrat :

```text
owner+0x15c
   ├─ vue d'index 3 : 0x822383d0 / 0x82238408
   ├─ résolution hiérarchique : 0x821d65c0
   ├─ copies de payload : 0x82386570 via 0x822a1258/0x822a9690
   └─ initialisation de sous-états : 0x82286210
```

Cela réduit encore la frontière inconnue : la phase statique est une
initialisation et une résolution de tables/états. Aucun de ces appels ne donne
une preuve directe de soumission de primitives ou de branche vers le modèle de
vol.

## Accès aux tables

Les deux helpers sont confirmés par décompilation :

```c
void *bounded_entry(View *view, int index) {
    if (index < view->count && view->offsets[index] != 0)
        return view->base + view->offsets[index];
    return 0;
}

uint32_t auxiliary_entry(View *view, int index) {
    if (view->count <= index)
        return 0;
    return view->auxiliary[index];
}
```

Ils correspondent respectivement à `0x822383d0` et `0x82238408`. L'index 3
observé dans le flux est donc une lecture contrôlée dans une vue déjà construite,
pas un accès arbitraire à une adresse de payload.

## Résolution hiérarchique `0x821d65c0`

`0x821d65c0` appelle `0x821d51e8` avec une clé et une liste chaînée à
`container+0x1c`. `0x821d51e8` :

1. parcourt les nœuds non nuls ;
2. compare la clé via le slot virtuel `+0x04` de chaque objet ;
3. retourne le premier nœud correspondant ;
4. en l'absence de correspondance, descend récursivement dans les enfants dont
   le slot virtuel `+0x0c` retourne `1`.

`0x821d65c0` retourne ensuite le premier mot du nœud trouvé, ou zéro. Le
contrat retenu est `hierarchical_key_lookup`, sans prétendre qu'il s'agit d'un
registre de matériaux, de scènes ou d'aéronefs.

## Copies de payload

`0x82386570` est une primitive de copie optimisée : elle traite les alignements
octet/mot/qword, touche les lignes de cache Xenon et retourne l'adresse de
destination. Les appels observés sont :

- `0x822a1258(param, source)` : si `source` est non nul, stocke `source` à
  `param+0x104`, puis copie `0x60` octets vers `param+0xa4` ;
- `0x822a9690(param, source)` : si `source` est non nul, copie `0x40` octets
  vers `param+0x04`.

Ces opérations sont des copies de structures de taille fixe. Elles ne
permettent pas de nommer le contenu comme `MATE`, `NDXR` ou `NTXR`.

## Initialisation de sous-état

`0x82286210(param, flag)` effectue, lorsque `flag != 0` :

```text
call 0x82284e88(param + 0x220)
store byte 1 at param + 0x4d0
```

`0x82284e88` n'est pas suffisamment désassemblée dans ce projet pour recevoir
un nom métier ; la conservation d'un contrat offset-qualified est obligatoire.

`0x821d4660` prépare par ailleurs un buffer temporaire sur la pile avec
`Function_823836D0`; ce chemin est un constructeur de contexte auxiliaire et
non une preuve d'un objet visuel.

## Relation avec le résultat `+0x15c`

Le flux `0x820abe14..0x820abf48` appelle ces helpers après :

1. la réserve/cursor `0x820ab4b0` ;
2. l'écriture à `owner+0x15c` ;
3. la réinjection du résultat dans le slot service `+0x10`.

La dépendance est donc ordonnée et reproductible. Elle ferme un contrat de
préparation de ressource et de copie de sous-états, mais pas l'usage final dans
le renderer.

## Confiance

`confirmed` :

- bornes et formules des vues `+0x00/+0x04/+0x10` ;
- recherche hiérarchique par clé et parcours des enfants ;
- tailles exactes des deux copies (`0x60`, `0x40`) et offsets de destination ;
- écriture du flag `param+0x4d0` ;
- ordre relatif après l'attachement type `0x98`.

`unknown` :

- type du payload copié ;
- sens du flag de sous-état ;
- effet interne de `0x82284e88` ;
- branche vers draw, scène ou modèle de vol.

## Validation

- `analyzeHeadless` en lecture seule avec `DecompileAt.java` ;
- exports absents reconstruits à la demande depuis le projet corrigé ;
- journaux `820abd80-service-init.log` et `820ab400-service-range.log` ;
- CTest AC6 : **41/41 PASS** ;
- aucune intervention humaine ou session Xenia/Wine/GUI.

## Suite

Le prochain front statique est la provenance des writers de `param+0x4d0`,
`param+0x104` et `param+0xa4`, ou la résolution d'un appelant de
`0x822c2148`. Une session humaine ne sera sollicitée que si les preuves
statiques ne permettent plus de départager ces rôles.
