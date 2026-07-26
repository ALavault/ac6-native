# AC6 cycle 195 — le tuple `0x3001` alimente un contexte d’interpolation

## Cible et méthode

- target : `ac6-xbox360-pal`
- module : `default.xex`
- SHA-256 : `acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde`
- image base : `0x82000000`

Passe statique, headless et en lecture seule. Le projet Ghidra et les sorties
générées n’ont pas été modifiés.

## Le consommateur `0x822aaeb8`

La décompilation de `0x822aaeb8` révèle la structure pointée par le second
argument (dans le cas étudié : `0x823fb360`) :

```text
+0x04 .. +0x10   quatre mots flottants de départ
+0x14 .. +0x20   quatre mots flottants d’arrivée
+0x24 .. +0x30   différence arrivée - départ, mise à l’échelle
+0x34            durée ou facteur temporel
+0x38            constante globale recopiée
+0x3c            statut/type fourni par l’appelant
+0x40            état, fixé ici à 3
```

Le helper recopie `param_6` dans `+0x04..+0x10` lorsqu’il est non nul,
recopie `param_5` dans `+0x14..+0x20`, puis calcule les quatre différences et
les multiplie par `1.0 / *(float *)(context + 0x34)`. Il s’agit donc d’un
contexte de transition/interpolation de quatre composantes, et non d’un simple
stockage opaque du tuple.

## Raccord avec le cas `0x3001`

Dans le dispatcher brut `0x8212b8ac` :

1. le record est résolu par son index et son offset relatif ;
2. `0x821265e8` extrait les quatre flottants ;
3. si le byte de contrôle `0x829188a0+0x16a6` est actif, ils sont écrits dans
   `0x823fb360+0x04..+0x10` ;
4. le byte `0x829188a0+0x24` est ensuite mis à `1`.

Le tuple `record_float_tuple` est donc le côté « départ/valeur courante » d’un
contexte de transition, avec notification à un objet de contrôle runtime. Les
types d’entrée `0x0011` et `0x8181` restent confirmés, mais les quatre
composantes ne peuvent pas encore être appelées position, quaternion, vitesse
ou paramètres de vol.

## Appels de transition et valeurs par défaut

Les appels à `0x822aaeb8` depuis `0x82250e18`, `0x822515d0`, `0x82251918`,
`0x82267dc0` et `0x82267e28` apparaissent dans des changements d’état ou de
cycle. Ils utilisent notamment :

- `0x82765b78`, quatre mots nuls, comme destination dans plusieurs chemins ;
- `0x823fb340`, statiquement `0,0,0,1`, comme autre quadruplet par défaut ;
- des durées/facteurs issus de constantes comme `0x82007e80` ou `0x82005eec`.

Ces valeurs renforcent l’interprétation « transition de paramètres/ressource »
mais ne suffisent pas à identifier un système de caméra ou l’avion joueur.

## Réinitialisation et cycle de vie

Les séquences `0x8218bf88`, `0x82255cf0`, `0x8226a498` et `0x8226bcac`
réinitialisent `+0x34`, `+0x10`, `+0x3c` et `+0x40`. Le lecteur autour de
`0x82255c38` teste `context+0x40` et appelle `0x82250080` lors d’une transition
de cycle. Cela relie le contexte à une gestion d’état, sans établir encore un
writer de pose, de caméra ou de mouvement de l’unité.

## Qualification et suite

- `confirmed` : layout partiel du contexte, calcul des différences, facteur
  temporel, raccord du cas `0x3001`, notification runtime ;
- `cross-match` : transition/interpolation de paramètres de record ;
- `unknown` / `needs-dynamic-evidence` : sémantique des composantes et
  propriétaire gameplay final.

La prochaine piste statique est de suivre les lecteurs de `+0x24..+0x40` dans
les chemins d’update et de vérifier si un writer de caméra ou d’unité existe.
Une trace runtime aidera seulement à donner un nom sémantique aux composantes;
aucune action humaine n’est requise pour cette étape.
