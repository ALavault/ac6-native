# Cycle 367 — la résolution GPU est réfutée : rien n'écrit jamais ces adresses

## 1. Mesure

Sonde sur le chemin de résolution (`VulkanCommandProcessor::IssueCopy` ->
`RenderTargetCache::Resolve`), journalisant l'adresse et la longueur
effectivement écrites, avec un compte global comme témoin.

```
résolutions journalisées (témoin) : 15, jusqu'à #2700
couvrant une planche de glyphes   : 0
destinations observées            : 1AB60000+398000  (invariable)
```

Toutes les résolutions visent la plage du tampon de trame. **Aucune ne couvre
`0x03514000` ni `0x028B7000`.**

L'hypothèse du cycle 366 — les planches sont produites par une résolution GPU
dont l'invalidation manque — est **réfutée**.

## 2. Ce que l'ensemble impose maintenant

Pour ces deux textures, il est désormais mesuré que :

| | |
|---|---|
| chargées | une fois chacune (365) |
| invalidées ensuite | **jamais** (366) |
| écrites par une résolution GPU | **jamais** (ici) |
| descripteur, format, liaison | sains, identiques aux textures qui marchent (362-364) |
| échantillon | **zéro** |

Si rien n'écrit ces adresses après le chargement, et que l'échantillon est nul,
alors **la mémoire était déjà vide au moment du chargement**. Le cache a
fidèlement capturé du vide.

La question se déplace donc hors du sous-système graphique : **pourquoi le
contenu des planches de glyphes n'est-il jamais produit ?**

C'est cohérent avec le cycle 337, où l'opérateur avait relevé que le dialogue
n'a pas de texte : il n'en a pas parce que **le texte n'est jamais généré**, pas
parce qu'il est mal rendu. Quinze cycles d'élimination graphique reviennent à ce
point de départ, mais avec la preuve que le chemin de rendu est sain.

## 3. Front suivant

La cause est en amont du GPU :

1. identifier quel code invité **écrit** `0x03514000` et `0x028B7000` sur
   matériel — l'oracle Xenia, disponible en headless (cycle 342), peut être
   instrumenté ou comparé ;
2. vérifier que ce code s'exécute chez nous. Le cycle 338 a montré qu'aucune
   lecture de fichier n'échoue ; il reste que la donnée peut n'être ni lue ni
   décodée par un chemin jamais atteint.

## 4. Bilan

Quinze causes graphiques éliminées, toutes par mesure avec témoin. Le rendu
n'est pas en cause. **Le défaut est en amont : la production du contenu.**

P1.3 non franchie. La première mission ne se joue pas.
`recompiler-generated` n'est pas `verified`.
