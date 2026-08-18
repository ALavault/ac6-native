# Qui lève l'événement (17, 6) : `CX360UnitManager`, slot `+0x14`

Date : 2026-08-18
Suite de `reports/AC6_DEMO_RENDER_GATE_CROSSCHECK.md`

## La recherche

Le rapport importé établit que la porte `device+0x5460` n'est armée que par le
callback `0x821ADAB8`, sur l'événement `(17, 6)`. Restait à trouver qui lève
cet événement.

Balayage de l'image entière pour la seule forme qui puisse le produire — un
`li rX,17` suivi à moins de huit instructions d'un `li rY,6` :

```text
candidats dans toute l'image : 1
```

Un seul : **`0x820A4778`**, dans la fonction `0x820A45E0`.

## Ce qu'il construit

```powerpc
0x820A4778  li   r11,17        ; 0x39600011
            stw  r11,0x98(r31)
            li   r11,13
            stw  r11,0x9C(r31)
            addi r11,r9,0x4138
            stw  r11,0xA0(r31)
            li   r11,6         ; 0x39600006
            stw  r11,0xA4(r31)
```

Un descripteur portant `event = 17` en `+0x98` et `channel = 6` en `+0xA4`,
avec un `13` et un pointeur entre les deux. C'est exactement la paire que
`0x821ADAB8` compare (`cmplwi r3,17` puis `cmplwi r4,6`).

## À qui appartient la fonction

```text
tools/whose_vtable.py .build/Default.xex.base.bin 0x820A45E0
    at 0x82000CC4   vtable 0x82000CB0 slot +0x14   CX360UnitManager  [RTTI]
```

L'armement du renderer est donc levé par une **méthode virtuelle du
gestionnaire d'unités**, pas par une couche graphique. Le même
`CX360UnitManager` que la pile de patches oracle borne en `0x8226FEC0`.

## État

| maillon | chez nous | oracle |
|---|---|---|
| `0x820A45E0` lève `(17,6)` | **non atteinte** | exécutée |
| `0x821ADC78` enregistre le callback | **atteinte** | exécutée |
| `0x821ADAB8` arme `device+0x5460` | non atteinte | exécutée |
| `sub_821C57D0` teste la porte, 5 463 fois | atteinte | exécutée |

Le seul maillon rompu est le premier. La chaîne complète, du pixel noir à sa
cause, tient maintenant en une phrase : `CX360UnitManager` n'appelle jamais sa
méthode `+0x14`, donc l'événement `(17, 6)` n'est jamais levé, donc la porte
reste nulle, donc la fonction par trame retourne aussitôt, 5 463 fois.

## Non établi

- Pourquoi le slot `+0x14` n'est pas appelé : `0x820A45E0` n'a aucun appelant
  statique, donc l'appel est virtuel et son site reste à trouver.
- Ce que valent les champs `13` et le pointeur `+0x4138` du descripteur.
