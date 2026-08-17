# AC6 — fermeture statique de l’IA aérienne et de WeaponBin

Date : 2026-08-17  
Corpus : démo Xbox 360 `ACE6_X360`  
Portée : analyse statique uniquement

> Révision : ce rapport remplace les anciennes conclusions relatives à `WeaponBin+0x5D` et au site `0x820A2C58`. Le détail du chemin de dommage est dans `AC6_DEMO_DAMAGE_EVENT_CLOSURE.md`.

## Résumé

Le contraste Strigon/F/A-18F/Rafale ferme l'origine de l'agressivité :

- les huit Su-33 Strigon utilisent un profil `ManeuverBin` distinct et identique entre eux ;
- ce profil contient les modes tactiques 4–8, contre 1–6 pour F/A-18F et Rafale ;
- les modes 7 et 8 sont exclusifs à Strigon ;
- les scalaires tactiques Strigon sont à `0.1`, contre `0.5/0.5/1.0` pour les modes 4–6 du F/A-18F ;
- les modes Strigon 5–8 ont 100 % de branches actives et 80–100 % de branches à commande directe d'axe.

Le canon principal des trois appareils est strictement identique octet pour octet. L'avantage Strigon est composite :

1. profil tactique plus agressif ;
2. deuxième slot d'arme, de famille missile ;
3. durabilité maximale 48 contre 40 ;
4. dommage nominal du missile 40 contre 8 pour le canon standard.

## Profils IA

| Modèle | Modes non vides | Branches | Commandes | Profil |
|---|---|---:|---:|---|
| F/A-18F | 1–6 | 63 | 157 | généraliste |
| Rafale M | 1–6 | 77 | 233 | généraliste plus actif |
| Su-33 Strigon | 4–8 | 35 | 88 | tactique élite |

Points discriminants :

- F/A-18F mode 4 : 60 % de branches explicitement réduites à l'attente ;
- Rafale mode 4 : 20 % ;
- Strigon modes 5 et 6 : 90 % de branches à commande directe pitch/roll/yaw ;
- Strigon mode 7 : 100 % direct-axis et 100 % manœuvre étagée ;
- Strigon mode 8 : 80 % direct-axis et 100 % manœuvre étagée.

Le reliquat de 40 % du mode 4 Strigon signifie « aucune nouvelle branche sélectionnée ». Il peut conserver l'état précédent et ne doit pas être nommé `idle`.

## Canon commun

Le record de 96 octets est identique pour F/A-18F, Rafale M et Su-33 :

```text
cea14157365087811e097ef7c6b5b3bb603b3f199135dcd4939348feffab06c4
```

| Champ | Valeur | Sémantique fermée |
|---|---:|---|
| `+0x00` | 800 | portée d'engagement/acquisition |
| `+0x04` | 1600 | distance maximale de parcours/lifetime |
| `+0x20` | 9.8 | accélération verticale/gravitée injectée négativement |
| `+0x24` | 0 | vitesse initiale relative au lanceur |
| `+0x28` | 4000 | vitesse cible/terminale relative au lanceur |
| `+0x2C` | 3° | dispersion directionnelle |
| `+0x30` | 15° | demi-angle du cône d'acquisition/tir |
| `+0x34` | 0.2 | intervalle de tir |
| `+0x38` | 1.5 | fenêtre de rafale, soit 7 tirs après troncature |
| `+0x5C` | 1 | famille canon |
| `+0x5D` | 8 | **terme de dommage de base** |

## Missile explicite Strigon

```text
965e6996991ea6575c1396199069b448f085bba4d0aee636bcd46e7f6519407d
```

| Champ | Valeur | Sémantique |
|---|---:|---|
| `+0x00` | 2500 | portée d'engagement |
| `+0x04` | 4000 | distance maximale de parcours |
| `+0x24` | 600 | vitesse initiale relative |
| `+0x28` | 2000 | vitesse cible/terminale relative |
| `+0x2C` | 2° | dispersion |
| `+0x30` | 30° | cône d'acquisition/tir |
| `+0x58` | 60 | paramètre angulaire converti en radians |
| `+0x5C` | 2 | famille missile/SAM |
| `+0x5D` | 40 | **terme de dommage de base** |

## Contrat WeaponBin

| Offset | Sémantique minimale |
|---:|---|
| `+0x00` | portée d'engagement/acquisition |
| `+0x04` | distance maximale de parcours/lifetime |
| `+0x0C/+0x10/+0x18` | composants du seuil temporel d'engagement |
| `+0x20` | accélération verticale/gravitée |
| `+0x24/+0x28` | vitesses initiale et cible/terminale relatives au lanceur |
| `+0x2C` | dispersion angulaire |
| `+0x30` | demi-angle du cône d'acquisition/tir |
| `+0x34/+0x38` | intervalle et fenêtre de rafale |
| `+0x54` | accélération optionnelle du projectile |
| `+0x58` | paramètre angulaire / limite de virage |
| `+0x5C` | famille d'arme |
| `+0x5D` | terme de dommage de base |

La formule de dommage exacte est :

```cpp
impact_magnitude = float(WeaponBin[0x5D]) + modifier_contextuel[10];
```

Le candidat historique `WeaponBin+0x10 = damage` reste réfuté : `+0x10` participe au seuil temporel :

```text
field_0x0C + field_0x10 + modifier4 * field_0x18
```

## Correction du faux producteur

`0x820A2C58` est un appel de décoration de texte radio utilisant `1008` comme masque de bits. Il ne produit aucun impact missile.

Le vrai chemin missile est :

```text
WeaponBin+0x5D
→ 0x82273470
→ projectile config+0x24
→ embedded+0x60
→ 0x821E9550
→ 0x821E2F60
→ cible, vslot+0x38
→ DurableBin
```

## État final

| Front | Verdict |
|---|---|
| Origine de l'agressivité Strigon | fermé |
| Canon Strigon différent | réfuté |
| Avantage matériel Strigon | missile supplémentaire + durabilité + profil tactique |
| Portées, vitesses, cône, dispersion, cadence | fermés |
| `WeaponBin+0x5D` | dommage de base, fermé A |
| formule de magnitude | `u8 + modifier[10]`, fermée A |
| application via DurableBin | fermée |
| nom métier du modificateur 10 | ouvert |
| producteurs sol/fragmentation complets | partiels |
