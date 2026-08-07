
# Cycle 1114 — la classe des deux vtables, et le nom de la machine

Date : 2026-08-09. La question ouverte du cycle 1113. La réponse nomme bien plus
que les deux tables.

## Qualification

- Projet Ghidra canonique `ghidra-projects/ace-combat-6`, Xbox 360 PAL
  `default.xex`,
  SHA-256 `acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde`.
- **Statique seul.**

## L'erreur du cycle 1113

J'avais écrit : « aucun localisateur RTTI ne précède la zone ». **C'était faux.**
Je cherchais les localisateurs dans l'arène `0x8206Dxxx`, celle des classes du
cycle 1096. Ce module range les siens en **`0x82076xxx`**. Une recherche calée
sur la mauvaise plage rend « aucun » et ne prouve rien — la même leçon qu'au
cycle 1106, apprise deux fois.

En balayant la bonne plage, deux localisateurs apparaissent entre les tables et
en donnent les bornes.

## Les deux tables

| vtable | localisateur | classe |
| --- | --- | --- |
| `0x820643EC` | `0x82076A00` | `ACE6::CAce6MissionManagerReplay` : `ACE6::CAce6MissionManager` |
| `0x820644B4` | `0x820769B0` | `CX360MissionManager<ACE6::CAce6MissionManagerReplay>` : les deux ci-dessus |

L'écart de `0xC8` relevé au cycle 1113 est exactement la distance entre les deux
bases, et les trois fonctions occupent **les mêmes emplacements relatifs** dans
chacune — 13, 14 et 47. Ce sont donc les méthodes virtuelles d'une classe et de
sa dérivée X360, celle du **rejeu**.

## Ce que la même zone révèle en plus

Les tables voisines nomment toute la famille :

| vtable | classe |
| --- | --- |
| `0x82064334` | `ACE6::Util::CHsm<ACE6::CAce6MissionManagerCampaign, 8>` |
| `0x82064384` | `CModeTaskGame` : `CModeTaskBase` : `ACE6::CAce6Task` : `CFsm<CModeTaskGame>` **(mdisp 0x268)** |
| `0x820643E4` | `ACE6::Util::CFsm<CModeTaskGame>` |
| `0x8206457C` | `CX360MissionManager<ACE6::CAce6MissionManagerCampaign>` : `CAce6MissionManagerCampaign` : `CAce6MissionManager` : **`CHsm<CAce6MissionManagerCampaign, 8>` (mdisp 0x348)** |

## Trois confirmations que je n'avais pas demandées

**1. La machine à états a un nom.** Le cycle 1109 décrivait `0x8219AD20` comme
« le moteur de transition d'une machine à états hiérarchique », lu à la forme de
l'algorithme. La RTTI dit **`ACE6::Util::CHsm`** — un modèle, instancié sur le
gestionnaire de mission. Et son voisin `CFsm` confirme que la maison distingue
machines plates et hiérarchiques.

**2. L'offset `0x348` vient du linker, pas de moi.** La RTTI place la base
`CHsm` à **mdisp `0x348`** dans `CX360MissionManager<…Campaign>`. C'est
exactement l'adresse que le moteur reçoit comme `this` (cycle 1109) et
l'ajustement présent dans chaque pointeur de membre (cycle 1111), établis
séparément par lecture d'instructions.

**3. Le paramètre `8` borne la profondeur.** `CHsm<…, 8>` porte un entier de
gabarit. L'arbre mesuré au cycle 1112 a une **profondeur maximale de 6** — sous
la borne, ce qui est cohérent sans être une preuve de sa signification.

## Le contexte de mission a enfin son nom

`0x8206457C` est installé par **`0x821A41D8`**, et cette fonction est l'un des
trois constructeurs que `0x82199F68` choisit selon le mode (cycle 1096, branche
par défaut). Donc :

> L'objet que ces rapports appellent « le contexte de mission » depuis le cycle
> 1093 est un **`CX360MissionManager<ACE6::CAce6MissionManagerCampaign>`**.

Ce qui range rétroactivement tout ce qui a été trouvé dessus : le code d'état en
`+0x260` et le pointeur d'enregistrement en `+0x264` appartiennent à
`CAce6MissionManager` ; la machine à états en `+0x348..0x360` est la base
`CHsm` ; les gestionnaires du cluster `0x822Exxxx` sont ses états.

## Ce que cela n'établit pas

- **Ce que font les trois méthodes**, au-delà d'écrire un code d'état : leur
  place dans la vtable du gestionnaire de **rejeu** est un fait, leur rôle non.
- Que `0x821AC368` — l'ambiguïté du cycle 1105, qui écrit `+0x270/0x274/0x278`
  depuis des globaux sans rapport — appartienne à la variante rejeu. C'est
  devenu plausible, la famille ayant maintenant des noms ; ce n'est pas montré.
- La signification du `8` de `CHsm<…, 8>`, ni celle du `0x268` de `CFsm` dans
  `CModeTaskGame`.
- La chaîne `UpTaskGame` de `0x8206421C` reste sans rattachement. Elle est
  voisine de `CModeTaskGame`, ce qui est suggestif et ne suffit pas.
