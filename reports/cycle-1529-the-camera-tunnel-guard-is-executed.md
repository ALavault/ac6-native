# Cycle 1529 — la garde tunnel caméra est exécutée

## Résultat

La suppression des axes mode 2 ne dépend plus d'un booléen synthétique seul.
Le produit exige maintenant l'état `manager+0x3C4==0`, le mode
`manager+0x190==1`, un `CGaObjDesc` non nul, son numéro de série live égal à
`manager+0x1A0`, puis seulement le résultat géométrique injecté. Une fixture
ancienne ou partielle échoue donc fermée et ne peut pas annuler les axes.

La géométrie tunnel reste une frontière explicite : les deux callbacks retail
réels et le fallback vertical ne sont pas encore portés en C++.

## Qualification

- Xbox 360 PAL, `default.xex`, SHA-256
  `acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde`.
- Projet canonique `ghidra-projects/ace-combat-6`, `-readOnly -noanalysis`.
- `G+0x362A8` référence `ACE6::CAce6TunnelManager`; `+4` contient au plus un
  tunnel et `+8` son compte.
- `0x82281198` exige le bit `tunnel+0x118 & 2`, appelle le slot virtuel
  `+0x12C` et accepte uniquement le byte retour égal à 1.
- Côté appelant `0x82262DA0..0x82262E20`, le pointeur `manager+0x19C`, le
  serial `objet+0xB0 == manager+0x1A0`, `+0x3C4==0` et `+0x190==1` précèdent
  la suppression.

## Contrôles exécutés

Trois fixtures synthétiques câblent le slot virtuel vers de petites feuilles
du XEX qui s'exécutent réellement; aucun appel n'est stubé et aucune sémantique
d'instruction n'est fournie :

```text
tunnel actif, callback 0x82266390 (retour 1)    45 étapes, r3=1
tunnel actif, callback 0x822663A8 (retour 0)    93 étapes, r3=0
bit actif absent, callback positive non appelée 82 étapes, r3=0
```

Les snapshots refusent sémantique affirmée, pont/alias de registres, écriture
mémoire ou dépendance au poison. L'auditeur scelle séparément le SHA des flags
et du pointeur callback pour chaque cas.

## Port natif et tests

`RetailMode2AxisSuppressionGuard` porte les quatre champs appelant qualifiés.
`should_suppress_mode2_camera_axes` compose ces champs avec le résultat déjà
calculé de `0x82281198`. Les sélecteurs direct et indirect l'appliquent tous les
deux. Les tests font varier séparément état, mode, pointeur, serial et résultat
tunnel, puis vérifient que l'ancien booléen seul ne contourne aucune garde.

## Validation

```text
build ciblé                                             pass
CTest caméra                                            1/1
camera selector microexec                               tunnel 45/93/82
tests Python caméra                                     11/11
ruff ciblé                                              pass
git diff --check                                        pass
```

## Frontières restantes

Les callbacks géométriques `0x82294190` et `0x822A93D8`, leurs fixtures retail,
le fallback `0x822131D0`, la publication runtime du manager et la copie VMX128
de la position live restent à porter. Le mode 1, le chemin VMX général et les
joins restants de `0x82262A28` empêchent toujours la fermeture Scene/TCAM.
