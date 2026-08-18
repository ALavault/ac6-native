# AC6 démo PAL — rétractation de la fausse « porte renderer »

Date : 2026-08-18  
Cible : image mémoire PAL de la démo, base `0x82000000`  
SHA-256 : `b81941994944b84f5792fd7b89cd698ca429b13c1bb4f501ea12e49dc54c2f01`

## Verdict

Quatre affirmations publiées précédemment doivent être retirées :

```text
0x820A45E0 produit un événement renderer (17, 6)                 faux
service 47 est un registre de callbacks propre au jeu            faux
device+0x5460 active ou désactive toute la frame graphique        faux
0x821ADD90 est le constructeur principal du packet de frame       faux
```

La chaîne correcte est :

```text
xboxkrnl!KeCertMonitorData
→ commande 47, catégorie 2, callback 0x821ADAB8
→ activation de canaux de certification / télémétrie
→ canal 6 : paire de marqueurs GPU optionnels autour de la frame
```

Le rendu normal continue lorsque `device+0x5460 == 0`. Le problème des deux
seules soumissions au ring ne peut donc pas être imputé à ce champ.

## 1. Origine de la première erreur : une table de factory lue comme un record événement

La fonction `0x820A45E0` initialise paresseusement une table de quinze records
de douze octets à `0x82661920`.

Le consommateur à `0x820A47D0..0x820A4820` établit le layout :

```cpp
struct FactoryRecord {
    uint32_t selector_key;       // +0
    Unit* (*constructor)(...);   // +4
    uint32_t runtime_tag;        // +8
};
```

L'algorithme :

```text
chercher selector_key == r5
→ appeler constructor
→ écrire runtime_tag dans object+0xB8
```

Les mots précédemment réunis en `(17, 6)` appartiennent à deux records voisins :

```text
record 12 : key=12, constructor=0x820A3F70, runtime_tag=17
record 13 : key=13, constructor=0x820A4138, runtime_tag=6
```

Il n'existe à cet endroit aucun descripteur contenant simultanément
`event=17` et `channel=6`.

L'ownership éventuel par `CX360UnitManager` peut rester valide ; la sémantique
« raiser renderer » ne l'est pas. Une vtable nomme un propriétaire, pas le
sens de trois mots adjacents.

## 2. Identité du service 47

`0x821ADC78` ne consulte pas un registre gameplay.

Il résout d'abord l'import variable :

```text
[0x82000610] = 0x00010266
```

L'ordinal `0x266` est `xboxkrnl.exe!KeCertMonitorData`.

Si cette variable n'est pas disponible, il utilise :

```text
[0x820006E4] = 0x00010059
```

soit `xboxkrnl.exe!KeDebugMonitorData`.

Les deux variables exposent un pointeur de callback. `0x821ADC78` construit :

```text
category = 2
callback = 0x821ADAB8
```

puis appelle ce callback noyau avec :

```text
command = 47
payload = &descriptor
```

Le même protocole est utilisé par XAudio :

```text
0x82359338 : command 47, category 4, callback 0x82369290
0x82359398 : command 47, category 4, callback null
```

Il s'agit donc d'un mécanisme partagé de certification / monitoring, et non du
bus d'événements de Mission 01.

Les wrappers voisins renforcent cette lecture :

```text
command 37 : envoi d'un petit record
command 38 : envoi d'une valeur entière
command 39 : publication d'une métrique flottante
command 47 : enregistrement / retrait d'un callback catégorisé
```

## 3. Rôle de `0x821ADAB8`

Le callback traite notamment :

| Événement | Effet |
|---:|---|
| `0`, `1` | remise à zéro des canaux |
| `16` | désactivation d'un canal |
| `17` | activation d'un canal |
| `34` | stockage d'une valeur auxiliaire |
| `224..226` | maintenance d'un objet de requête |
| `255` | republication de toutes les métriques actives |

Les événements `16/17` mettent à jour le masque global :

```text
0x827AD2F4
```

Le canal `6` contrôle :

```text
device+0x5460
device+0x5458
device+0x545C
```

Le census de tous les stores D-form du `.text` trouve exactement :

```text
device+0x5460 :
    0x821ADB40  activation
    0x821ADB74  désactivation

global mask -0x2D0C :
    0x821ADB34
    0x821ADB68
    0x821ADB84
    0x821ADD0C
```

Il n'existe aucun writer gameplay indépendant.

## 4. L'événement 255 ne peut pas amorcer le canal 6

L'événement `255` parcourt 17 records à `0x823C2EA8`.

Chaque record contient :

```text
enable_mask
metric_token
channel
```

Pour chaque bit déjà actif, il :

1. interroge la métrique avec `0x821ACE28` ;
2. la publie avec le wrapper de commande 39.

Le record du canal 6 est :

```text
mask   0x00000040
token  0x00020006
channel 6
```

Mais l'événement 255 ne positionne aucun bit. Il rejoue seulement les canaux
déjà activés. Avec un masque initial nul, il n'ouvre rien.

## 5. `device+0x5460` ne bloque pas la frame

Dans `0x821C57D0` :

```text
0x821C5878  lit device+0x5460
0x821C58B4  si nul, branche vers 0x821C5920
```

Or `0x821C5920` n'est pas un épilogue :

```powerpc
cmplwi r26, 0
```

et la fonction continue pendant près de trois cents instructions, notamment :

```text
0x821C59AC  appelle 0x821C07F8
0x821C59D4  alloue / prépare des structures
0x821C5A18  remplit une commande ou un descripteur
0x821C5AB8  poursuit le chemin device
0x821C5C30  soumet une autre branche de travail
```

La partie protégée par `device+0x5460` est seulement :

```text
réserver deux slots de requête
→ écrire un marqueur avant la frame
→ exécuter le travail normal
→ écrire un marqueur après la frame
→ avancer l'index de requête de deux
```

Le flag local `r29` relie explicitement les deux marqueurs.

## 6. `0x821ADD90` est un writer de marqueur

La fonction n'écrit que quatre dwords :

```text
0xC0025800
0x80000003
adresse encodée
0xDEADBEEF
```

Le premier mot est un packet type 3, opcode `0x58`, nommé
`EVENT_WRITE_EXT` dans les sources Xenos de Xenia.

Elle n'appelle :

- aucun import noyau ;
- aucun `KeSetEvent` ;
- aucun writer MMIO du ring ;
- aucun constructeur d'unité ;
- aucun dispatcher de mission.

Elle possède exactement deux callsites directs dans toute l'image :

```text
0x821C5918  marqueur d'ouverture
0x821C5A58  marqueur de fermeture
```

Cela ferme sa fonction minimale : instrumentation GPU / requête de mesure
facultative autour de la frame.

## 7. Nouvelle frontière renderer

La mauvaise chaîne était :

```text
CX360UnitManager absent
→ événement (17,6) absent
→ gate renderer nul
→ aucune frame
```

La chaîne corrigée est :

```text
SWG et frontend exécutent leurs updates
→ 0x821C57D0 poursuit le chemin normal même sans monitoring
→ des commandes ou structures sont produites
→ mais la publication au command processor / ring ne progresse pas
```

Le front utile devient :

```text
commande construite
→ owner de la command list
→ fermeture / enqueue du buffer
→ worker ou handoff
→ écriture du ring
```

La prochaine instrumentation ne doit plus surveiller `device+0x5460`. Elle doit
prendre le premier buffer écrit après `0x821C5920` et suivre :

```text
base
write cursor
tail cursor
owner
appel de fermeture
enqueue
ring write
```

## 8. Rétractations explicites

Ce rapport supplante les passages correspondants de :

```text
reports/AC6_DEMO_PAL_RENDER_EVENT_GATE_CLOSURE.md
reports/AC6_DEMO_RENDER_GATE_CROSSCHECK.md
reports/AC6_DEMO_RENDER_SUBMISSION_DIVERGENCE.md
```

et toute conclusion déduisant un événement `(17,6)` de `0x820A4778..0x820A4794`.

Les mesures « ring inchangé » restent valides. Leur cause précédente ne l'est
pas.

## Verdict

| Claim | Verdict |
|---|---|
| `0x820A45E0` est une factory selector | **fermé A** |
| `17` et `6` appartiennent au même événement | **réfuté A** |
| commande 47 passe par `KeCertMonitorData` | **fermée A** |
| catégorie 2 enregistre `0x821ADAB8` | **fermée A** |
| canal 6 contrôle un bracket de requête GPU | **fermé A-** |
| `device+0x5460` bloque toute la frame | **réfuté A** |
| `0x821ADD90` construit la frame | **réfuté A** |
| ring inchangé sur les runs observés | **inchangé** |
| frontière exacte command-list → ring | **ouverte** |

## Audit adversarial

- `KeCertMonitorData` établit la nature certification/monitoring du transport ;
  les noms internes des commandes 37/38/39/47 restent absents.
- L'opcode `0x58` qualifie un marqueur GPU ; le type exact de métrique écrite
  à l'adresse encodée reste ouvert.
- Le rôle de factory de `0x820A45E0` est indépendant de son nom de classe.
- La continuation après `0x821C5920` prouve que le gate n'annule pas la frame ;
  elle ne prouve pas que chaque branche ultérieure produit un draw.
- Le problème de ring reste réel, mais il doit être recherché après la
  construction des commandes, pas dans le monitor de certification.
