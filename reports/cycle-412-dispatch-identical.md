# Cycle 412 — la sonde fonctionne ; l'aiguillage est **identique** sur les deux écrans

## 1. Correction effectuée

Le vrai appelant de `sub_8234D3F0` dans `generated/` — l'arbre que la
compilation consomme — est **`sub_8234D510`**, et non `sub_8234D50C`. Les
frontières de fonctions diffèrent de 4 octets entre les deux arborescences. Les
cycles 408 à 411 ont été menés sur l'arbre périmé ; la sonde visait une fonction
voisine qui ne s'exécute jamais, d'où son silence.

Sonde redirigée sur `0x8234D510`, régénérée, reliaison vérifiée **avant**
exécution (`ac6recomp_init.cpp:17446` → `rex_sub_8234D510`). Elle émet.

## 2. Mesure

| objet | branche | appels (écrans qui marchent) | appels (écran bloqué) |
|---|---|---|---|
| `0x8290DE3C` | même | 1050 | 3300 |
| `0x8290DEC4` | même | 1050 | 3300 |
| `0x8290DF4C` | même | 1050 | 3300 |
| `0x8290DFD4` | même | 1050 | 3300 |

Quatre objets régulièrement espacés de 0x88 — la forme de quatre emplacements
de manette. Tous empruntent **la même branche**, et les compteurs croissent
**au même rythme** avant et pendant le blocage.

## 3. Résultat : négatif, et net

**L'aiguilleur d'entrée se comporte exactement pareil sur un écran qui
fonctionne et sur l'écran bloqué.** Aucune différence : ni d'objet, ni de
branche, ni de cadence.

L'hypothèse du cycle 409 — un gestionnaire par écran, absent ou différent sur
l'écran bloqué — n'est **pas** confirmée à ce niveau. Le point de divergence est
plus loin en aval.

## 4. Réserve sur l'étiquetage

Les noms de branches de la sonde (`init`, `poll`, `NO-OP`) proviennent de la
désassemblage lue dans **l'arbre périmé**. La branche relevée ici est étiquetée
`init(sub_8234D478)` mais compte 3300 appels, ce qui contredit l'unicité
attendue d'une initialisation. **L'étiquette est donc probablement fausse** ; le
fait mesuré — une seule et même branche, aux mêmes cadences, sur les deux
écrans — ne dépend pas d'elle.

À corriger en relisant la condition de branchement dans `generated/` avant de
réutiliser ces noms.

## 5. Où chercher ensuite

En aval de `sub_8234D3F0` : la valeur lue est correcte, l'aiguillage est
correct, la cadence est correcte. Ce qui diffère se situe entre la lecture du
masque de boutons et la transition d'écran qui devrait s'ensuivre.

P1.3 non franchie. La première mission ne se joue pas.
`recompiler-generated` n'est pas `verified`.
