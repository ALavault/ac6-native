# AC6 démo — fermeture du chemin impacts, dommages et DurableBin

> **Qualification d’intégration.** Importé de `origin/infos` (`e6362d1e`). Le
> cœur statique `WeaponBin+0x5D + modifier[10] → event+0x0C → DurableBin` est
> recroisé avec le basefile PAL `b98a9ac1…14218` et le projet
> `ace-combat-6-demo` dans `cycle-1763-ac6-demo-infos-branch-audit`. Les deux
> getters `0x8216B258/0x8216B3A8` restent sans décompilation qualifiée et la
> chaîne complète n’a pas de preuve dynamique. Les grades historiques ne
> promeuvent donc aucun gate runtime, frontend ou mission.

Date : 2026-08-17  
Corpus : démo Xbox 360 `ACE6_X360`  
Portée : analyse statique uniquement ; toutes les adresses sont préfixées implicitement `demo:`

## Résumé

La chaîne des dommages est fermée pour les armes à projectile :

```text
WeaponBin+0x5D
  + modificateur contextuel d'identifiant 10
    → magnitude flottante
      → configuration du projectile
        → callback d'impact
          → événement compact
            → unité cible, vslot +0x38
              → DurableBin[type]
                → durabilité courante
```

La formule exacte est :

```cpp
impact_magnitude =
    float(weapon_bin->base_damage_u8) +
    context_modifier_table[context][10];
```

Sans service de modificateurs :

```cpp
impact_magnitude = float(weapon_bin->base_damage_u8);
```

Le byte `WeaponBin+0x5D`, anciennement décrit comme « flags/sous-mode », est donc un **terme de dommage de base**. Le canon aérien standard porte 8 ; le missile explicite de Strigon porte 40.

## Rétractation du faux positif radio

Le site `0x820A2C58` ne produit pas `MISSILEDAMAGE`. Il appartient à `CX360RadioPlayManagerOnline` et appelle un `CX360RadioTextDecorate` embarqué. La valeur `1008` y est un masque de décoration de texte.

Il n'existe à ce site ni cible d'unité, ni magnitude, ni structure d'impact, ni accès à `DurableBin`.

Le mapping indépendant reste valide :

```text
1008 → MISSILEDAMAGE → DurableBin+0x18
```

## Extraction depuis WeaponBin

La fonction `0x82273470` :

1. récupère le service contextuel éventuel ;
2. appelle `0x822B6FE0` avec le modificateur `10` ;
3. résout le slot `WeaponBin` sélectionné ;
4. charge `lbz WeaponBin+0x5D` ;
5. convertit l'octet non signé en flottant ;
6. additionne le modificateur.

Instructions décisives :

```text
0x822734C0  li    r4, 10
0x822734C8  bl    0x822B6FE0
0x82273500  lbz   r11, 0x5D(r11)
0x8227350C  fcfid f0, f0
0x82273514  fadds f1, f0, f1
```

Le rôle additif du modificateur est exact ; son nom métier demeure ouvert.

## Canon / projectile balistique

`0x822735A8` appelle l'extracteur puis stocke le résultat à `Arms+0x168`.
Le callback d'impact `0x822885C8` atteint un getter concret qui remet `Arms+0x168` dans `f1`, puis appelle le constructeur compact `0x821E2F60`.

```text
WeaponBin+0x5D
→ Arms+0x168
→ f1 au callback d'impact
→ compact_event+0x0C
```

## Missile

Le configurateur `0x822A4978` copie le template cinématique `0x82645EC8`, puis remplace notamment :

```text
config+0x00  vitesse initiale
config+0x04  vitesse cible/terminale
config+0x08  accélération optionnelle
config+0x18  limite angulaire
config+0x1C  paramètre cinématique
config+0x24  magnitude de dommage
```

`config+0x24` reçoit le résultat de `0x82273470`.

La chaîne fermée est :

```text
0x822A4978
→ 0x82286830
→ embedded projectile+0x60
→ 0x8216B258
→ outer getter 0x8216B3A8
→ missile collision callback 0x821E9550
→ compact event builder 0x821E2F60
```

Le RTTI rattache ce chemin à `ACE6::ARMS::CAce6Missile`, `CAce6ArmsMissile` et `CX360ArmsMissile`.

## Événement compact d'impact

`0x821E2F60` produit 44 octets :

| Offset | Contenu |
|---:|---|
| `+0x00` | identifiant complet d'événement depuis `source+0xB4` |
| `+0x04` | sous-type / slot Arms depuis `source+0x10C` |
| `+0x08` | identifiant auxiliaire depuis `source+0x104` |
| `+0x0C` | magnitude `f1` |
| `+0x14` | identité de la cible |
| `+0x18` | identité de la source |
| `+0x1C` | vecteur ou position d'impact |

`0x821E23B8` résout ensuite la cible et invoque son slot virtuel `+0x38`.

## DurableBin

`0x8224FE60` applique :

```cpp
target->current_durability -=
    target->durable_bin[event_code] * event->magnitude;
```

Champs confirmés :

```text
target+0x110  durabilité courante
target+0x114  durabilité maximale de repli
event+0x0C    magnitude
```

Événements d'arme connus :

| Code | Nom | Coefficient |
|---:|---|---:|
| 1007 | `GUNDAMAGE` | `DurableBin+0x14` |
| 1008 | `MISSILEDAMAGE` | `DurableBin+0x18` |
| 1009 | `GROUNDDAMAGE` | `DurableBin+0x1C` |
| 1014 | `FRAKEDAMAGE` | `DurableBin+0x30` |

## Dispatcher commun

`0x821ED088` copie un événement complet de `0x84` octets, lit son ID à `+0x60`, soustrait 1003 puis utilise une table de branchement couvrant 21 codes.

Ce bus commun explique pourquoi `DurableBin` réunit armes, contact au sol et états aérodynamiques : ce n'est pas une simple table d'armure, mais une table de sensibilité aux événements.

## Familles partageant la formule

`0x82273470` possède sept callers directs :

```text
0x822735A8
0x822A37A8
0x822A4978
0x822A52B0
0x822A80E8
0x822A8390
0x822A87F0
```

Ils couvrent canon, missile et plusieurs projectiles spéciaux. Tous utilisent donc `base_damage_u8 + modifier[10]`.

## Verdict

| Claim | Verdict |
|---|---|
| `WeaponBin+0x5D` est le terme de dommage de base | **fermé A** |
| formule `base + modifier[10]` | **fermée A** |
| transport canon jusqu'à l'événement | **fermé A-** |
| transport missile jusqu'à l'événement | **fermé A** |
| formule finale via `DurableBin` | **fermée A** |
| `0x820A2C58` produit `MISSILEDAMAGE` | **réfuté** |
| magnitude dérivée de l'énergie cinétique | **réfuté pour les chemins qualifiés** |
| owner exact de tous les producteurs sol/frak | **partiel** |
| nom métier du modificateur 10 | **ouvert** |

## Frontières résiduelles

- joindre les descriptors chargés depuis les ressources aux codes 1007/1008/1009/1014 pour chaque classe productrice ;
- nommer le modificateur 10 ;
- identifier les événements 1018–1023.

Le prochain sous-système prioritaire devient l'orchestration `ActBin / OrderBin / SetBin`, afin de fermer Nimbus, les chars aéroportés, les vagues et la retraite.
