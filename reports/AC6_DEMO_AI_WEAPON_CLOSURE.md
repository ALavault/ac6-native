# AC6 — fermeture statique de l’IA aérienne et de WeaponBin

Date : 2026-08-17  
Corpus : démo Xbox 360 `ACE6_X360`; adresses préfixées `demo:`  
Portée : analyse statique uniquement

## Résumé exécutif

Le contraste Strigon/F/A-18F/Rafale ferme la question de l’agressivité :

- les huit Su-33 Strigon utilisent un profil `ManeuverBin` distinct et identique entre eux ;
- ce profil ne contient que les modes tactiques 4–8, alors que F/A-18F et Rafale utilisent les modes 1–6 ;
- les modes 7 et 8 sont exclusifs à Strigon ;
- les scalaires tactiques Strigon sont tous à `0.1`, contre `0.5/0.5/1.0` pour les modes 4–6 du F/A-18F ;
- les modes Strigon 5–8 ont 100 % de branches actives et 80–100 % de branches à commande directe d’axe ;
- aucun mode tactique Strigon ne contient de branche explicitement réduite à l’opcode d’attente seul.

L’agressivité observée n’est pas produite par un canon plus puissant. Le `WeaponBin[0]` du Su-33 est **strictement identique octet pour octet** à celui des F/A-18F et Rafale M de la mission. L’avantage Strigon est composite :

1. profil tactique plus agressif ;
2. deuxième slot d’arme, un missile ;
3. durabilité maximale sérialisée de 48, contre 40 pour F/A-18F/Rafale.

Pour `WeaponBin`, les frontières targeting, trajectoire, cadence et dispersion sont maintenant fermées. Le dommage est une frontière externe : le candidat `+0x10` est réfuté comme champ de dommage, car il participe à un seuil temporel d’engagement. La magnitude de dommage est produite par le sous-système impact/collision puis transportée par l’événement.

## 1. Profil IA Strigon

### 1.1 Profils exacts

| Modèle | Modes non vides | Branches | Commandes | Profil |
|---|---|---:|---:|---|
| F/A-18F | 1–6 | 63 | 157 | généraliste |
| Rafale M | 1–6 | 77 | 233 | généraliste plus actif |
| Su-33 Strigon | 4–8 | 35 | 88 | tactique élite |

Les profils sont partagés exactement entre tous les objets du même modèle dans l’échantillon de mission. Les huit Strigon partagent le SHA-256 structuré `f0a326c209cae6db003cfb7626d13efe1f2bd43cb551b61736138f8093d29769`.

### 1.2 Statistiques pondérées

Le sélecteur `demo:0x82247DB8` tire un entier modulo 100 et sélectionne la première branche dont le cumul de poids dépasse le tirage. Les statistiques de `tables/strigon_mode_metrics.csv` reproduisent donc le comportement du sélecteur, y compris les poids supérieurs ou inférieurs à 100.

Points discriminants :

- F/A-18F mode 4 : 60 % de branches explicitement « attente seulement », 40 % de branches actives.
- Rafale mode 4 : 20 % d’attente seule, 80 % actives. Il est déjà plus offensif que le F/A-18F.
- Strigon mode 4 : aucune branche d’attente seule ; le total de poids vaut 60, laissant 40 % sans nouvelle sélection. Cette fraction conserve vraisemblablement l’état précédent, elle ne doit pas être renommée « idle ».
- Strigon modes 5 et 6 : 100 % de branches actives, 90 % avec commande directe pitch/roll/yaw.
- Strigon mode 7 : 100 % actives, 100 % direct-axis et 100 % manœuvre étagée.
- Strigon mode 8 : 100 % actives, 80 % direct-axis et 100 % manœuvre étagée.

La conclusion la plus parcimonieuse est que Strigon réévalue plus rapidement ses états tactiques et dispose de deux états supplémentaires spécialisés. Son comportement agressif n’est pas une simple hausse globale d’un coefficient.

## 2. Comparaison des armes

### 2.1 Canon standard

Le record canon de 96 octets est identique pour les trois familles :

`cea14157365087811e097ef7c6b5b3bb603b3f199135dcd4939348feffab06c4`

Valeurs principales :

| Champ | Valeur | Sémantique fermée |
|---|---:|---|
| `+0x00` | 800 | portée d’engagement/acquisition |
| `+0x04` | 1600 | distance maximale de parcours/lifetime du projectile |
| `+0x20` | 9.8 | accélération verticale/de gravité injectée négativement |
| `+0x24` | 0 | vitesse relative initiale |
| `+0x28` | 4000 | vitesse relative cible/terminale |
| `+0x2C` | 3° | dispersion directionnelle |
| `+0x30` | 15° | cône d’acquisition/tir |
| `+0x34` | 0.2 | intervalle de tir |
| `+0x38` | 1.5 | fenêtre de rafale, soit `trunc(1.5/0.2)=7` tirs minimum borné à 1 |
| `+0x5C` | 1 | famille canon |
| `+0x5D` | 8 | flags/sous-mode |

Il n’existe donc aucun bonus de canon propre à Strigon dans ces objets.

### 2.2 Missile Strigon

Le deuxième slot des huit Su-33 contient le profil :

`965e6996991ea6575c1396199069b448f085bba4d0aee636bcd46e7f6519407d`

| Champ | Valeur | Interprétation |
|---|---:|---|
| `+0x00` | 2500 | portée d’engagement |
| `+0x04` | 4000 | distance maximale de parcours |
| `+0x24` | 600 | vitesse relative initiale |
| `+0x28` | 2000 | vitesse relative cible/terminale |
| `+0x2C` | 2° | dispersion |
| `+0x30` | 30° | cône d’acquisition/tir |
| `+0x58` | 60 | paramètre de vitesse angulaire/limite de virage, converti en radians |
| `+0x5C` | 2 | famille missile/SAM |
| `+0x5D` | 40 | flags/sous-mode |

Dans la démo, les F/A-18F et Rafale de ces vagues n’ont pas de deuxième `WeaponBin`. Le bénéfice matériel Strigon provient donc de ce missile additionnel et de la durabilité supérieure, pas du canon.

## 3. Contrat WeaponBin fermé

### 3.1 Enveloppe de tir

`demo:0x82252778` parcourt les trois slots `ObjBin+0x10/+0x14/+0x18`.

- `WeaponBin+0x00` est multiplié par les modificateurs de l’arme et de la cible, mis au carré, puis comparé à la distance au carré. C’est la portée d’engagement/acquisition.
- `WeaponBin+0x30` est multiplié par un modificateur, transformé par `demo:0x82328BF8` et comparé au produit scalaire directionnel. C’est le demi-angle du cône d’acquisition/tir.

La distinction est donc nette : une arme peut disposer d’une portée d’engagement différente de la distance maximale parcourue par son projectile.

### 3.2 Cinématique

Les getters `demo:0x822A4698`, `0x822A48A8` et `0x822A47F8`, puis `demo:0x822A4978`, alimentent le solveur cinématique `demo:0x82286410` :

- `+0x04` : distance maximale de parcours / portée de lifetime ;
- `+0x24` : vitesse initiale relative au lanceur ;
- `+0x28` : vitesse cible/terminale relative au lanceur ;
- `+0x54` : accélération optionnelle ;
- `+0x58` : paramètre angulaire converti degrés → radians.

Le terme « relative au lanceur » est préféré à « vitesse monde » : le projectile est initialisé depuis le mouvement du lanceur puis reçoit ces paramètres propres.

### 3.3 Cadence et cycle d’engagement

Dans `demo:0x822735A8` :

`burst_count = max(1, trunc(field_0x38 / field_0x34))`

Donc `+0x34` est le pas/intervalle de tir et `+0x38` la fenêtre ou durée de rafale.

Dans `demo:0x82252778`, le seuil du timer est exactement :

`field_0x0C + field_0x10 + modifier4 * field_0x18`

Le champ `+0x10` ne peut donc pas être promu en dommage. Sa distribution numérique était séduisante, ce qui est précisément le genre de détail qui fait perdre plusieurs jours lorsqu’on préfère la poésie des nombres à leurs consommateurs.

## 4. Frontière du dommage

Le chemin aval est fermé :

1. `demo:0x822B40C0` construit un événement et stocke sa magnitude `f1` à `event+0x80` ;
2. `demo:0x8224FE60` applique :

   `current_durability -= DurableBin[event_code] * event_magnitude`

3. la collision missile dans `demo:0x820A2698` passe explicitement le code `1008` (`MISSILEDAMAGE`) à `demo:0x820A2C58`.

En revanche, le site de collision ne charge pas un flottant `WeaponBin` comme magnitude. Aucun consommateur direct qualifié de `WeaponBin` ne relie `+0x10` ou un autre champ au `f1` de l’événement.

Verdict :

- `WeaponBin` décrit l’acquisition, la trajectoire, la cadence, la dispersion et la famille ;
- la magnitude de dommage est produite par un sous-système impact/collision/effet distinct ;
- l’absence de champ de dommage directement démontré dans `WeaponBin` est une fermeture de frontière, pas la preuve métaphysique qu’aucune table auxiliaire n’en contient un.

## 5. État final

| Front | Verdict |
|---|---|
| Origine de l’agressivité Strigon | fermé : profil tactique, modes 7–8 et scalaires 0.1 |
| Canon Strigon différent | réfuté : record strictement identique |
| Avantage matériel Strigon | fermé : missile additionnel + durabilité 48/40 |
| Portée d’engagement | fermé : `WeaponBin+0x00` |
| Portée de parcours projectile | fermé : `WeaponBin+0x04` |
| Vitesses relatives | fermé : `+0x24/+0x28` |
| Cône de tir | fermé : `+0x30` |
| Dispersion | fermé : `+0x2C` |
| Cadence/rafale | fermé : `+0x34/+0x38` |
| Candidat dommage `+0x10` | réfuté : composant temporel |
| Application des dégâts | fermée en aval via événements et `DurableBin` |
| Production de magnitude d’impact | frontière du sous-système suivant |

## Limites

- Les noms exacts des modes 4–8 ne sont pas présents ; leur caractère tactique vient de leurs prédicats, de leur sélecteur et de leurs commandes.
- Le reliquat de 40 % du mode 4 Strigon signifie « aucune nouvelle branche sélectionnée », pas nécessairement « inactif ».
- `+0x58` est bien converti en radians, mais son nom ABI exact reste à établir.
- Les champs encore neutres sont listés dans `tables/weaponbin_closed_layout.csv`; ils ne sont pas remplis à coups de synonymes plausibles.
- Toutes les adresses sont propres au XEX de la démo.
