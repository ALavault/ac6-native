# C'étaient des compteurs, pas un mode d'affichage — correction de `5da91f72`

Date : 2026-08-18

## Ce que j'avais publié

> `[0x827AD2F0] = 0` … `sub_821AD378` est un `switch` à neuf cas sur
> `[0x827AD2F0] - 11` … **le mode d'affichage n'est jamais posé, et c'est ce
> qui bloque l'anneau.**

Les **valeurs** mesurées restent exactes. L'interprétation « mode d'affichage »
était une conjecture sur le nom, et elle est fausse.

## Ce que Microsoft nomme

Plancher de bruit d'abord, à fenêtre 32 sur 40 fenêtres, mesuré sur deux
fonctions certainement Namco (`CModeTaskTitle::update` et `sub_820E8F90`) :
**0/40 pour les deux**. Toute correspondance à ce réglage est donc
significative.

```text
0x821ACCD0  D3D::GetCounter(CDevice*, _D3DCOUNTER)              29/200 d3d9
                                                                 8/200 d3d9i
0x821BE9A0  D3D::XBMProcessCommand(const char*,char*,DWORD,_DM_CMDCONT*)
                                                                21/200 d3d9
                                                                14/200 d3d9i
0x821C64E8  D3D::InitializeHardwareDevice(CDevice*, const D3DPRESENT_PARAMETERS*)
                                                                 4/200 d3d9 et d3d9i
```

`0x821ACCD0` — l'un des deux écrivains de `0x827AD2F0` — est **`GetCounter`**.
Le global n'est donc pas un mode d'affichage : c'est un **sélecteur de compteur
de performance**, du type `_D3DCOUNTER`. Le `switch` à neuf cas de
`sub_821AD378`, qui écrit `[device+21508]`, est un aiguillage **par type de
compteur**, et `[device+21508]` est un champ de compteur.

C'est le même verdict que pour `device+0x5460`, dont `1a5c4f76` a montré qu'il
appartenait à `D3D::CounterHandler`. Deux frontières poursuivies par cette
campagne se révèlent être la même chose : de l'instrumentation de performance
qui, sans profileur attaché, ne fait rien — correctement.

## Ce que la mesure dit vraiment

`sub_821B9BC8` — `D3D::CDevice::AddCallsToPrimaryBuffer` — a deux chemins :

```text
[device+10941] & 0x2 == 0  ->  loc_821B9C6C   travail réel
                               (son bctrl a le LR 0x821B9C80, celui des
                                trois écritures d'anneau du tick 0)
[device+10941] & 0x2 != 0  ->  test [device+21508], puis loc_821B9DA4
                               qui est l'épilogue : restaurer et rendre
```

Mesuré : l'octet vaut `0x06`, donc le bit est **posé**, sur les 47 238 appels.

Et le poseur est nommé : **`D3D::CDevice::KickOff()`** (`sub_821BA780`,
23 619 appels) fait `[device+10941] |= 2`.

## Le fait exhaustif

Recherche du masque `& 253` — le seul qui efface le bit 1 — dans **tout** le
C++ généré : deux sites, `sub_821ABDD0` sur `[r30+5]` et `sub_821BB290` sur
`[device+10942]`. **Aucun ne touche `+10941`.**

Rien dans l'image n'efface ce bit. Il est posé au premier `KickOff` et le
reste.

## Ce que je ne conclus pas

Que ce soit le défaut. Un bit posé une fois et jamais effacé, dans une
bibliothèque Microsoft largement déployée, est plus vraisemblablement **normal**
qu'un bogue — et si le chemin bit-posé est le chemin normal après le premier
`KickOff`, alors `loc_821B9DA4` n'est pas un échec mais un retour légitime, et
l'anneau est publié ailleurs.

La conséquence pratique : la chaîne causale de `5da91f72` est **rompue au
niveau de son interprétation**, et je ne la remplace pas par une autre ici.

## Non établi

- Où la publication d'anneau a réellement lieu quand le bit est posé.
- Le nom de `sub_821AD378` et `sub_821AD7C0` — toujours aucun, à un réglage
  dont le plancher est pourtant nul.
- Si `0x821C64E8` est bien `InitializeHardwareDevice` : 4/200 est faible, mais
  identique dans deux archives et cohérent avec son usage de `VdGlobalDevice`.
