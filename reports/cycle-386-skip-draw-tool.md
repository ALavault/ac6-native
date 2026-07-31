# Cycle 386 — l'outil qui relie un dessin à des pixels existe ; le protocole manque

## 1. Outil ajouté

`--ac6_skip_texture_base=0xXXXXXXXX` : dans `VulkanCommandProcessor::IssueDraw`,
tout dessin dont le pixel shader lie la base de texture indiquée est **omis**,
en rendant succès. Les pixels qui disparaissent sont exactement ceux que cette
texture dessine.

C'est la mesure réclamée au cycle 385 — la première capable de relier un lot de
dessin à une **zone de l'écran**, ce qu'aucune des vingt-sept éliminations n'a
jamais fait.

## 2. Ce que la première tentative donne

Deux exécutions, l'une témoin, l'autre avec `0x03514000` omis :

```
pixels modifiés : 613 660 sur 921 600  (66 %)
boîte englobante : (0,0)-(1279,719)     plein écran
```

**Inexploitable.** Les deux exécutions n'ont pas abouti au même état : l'écart
couvre tout l'écran, il est dominé par la divergence entre exécutions et non par
l'effet de l'omission.

C'est le même piège qu'au cycle 334, où le plancher de bruit témoin-contre-témoin
valait 116 pour un effet de 36. La leçon n'avait été appliquée qu'aux captures
animées ; elle vaut aussi ici, où deux parcours identiques ne produisent pas le
même état.

## 3. Ce qu'il faut pour que la mesure porte

Il faut comparer **à l'intérieur d'une même exécution**, pas entre deux :

- rendre `ac6_skip_texture_base` modifiable en cours d'exécution, capturer avant
  et après bascule ; ou
- omettre le dessin **une trame sur deux** et comparer deux trames consécutives.

La seconde est la plus simple et ne dépend d'aucune infrastructure de cvar
dynamique : un compteur de trames dans la condition d'omission suffit.

## 4. État

L'outil est construit, compilé et commité ; le protocole qui le rend concluant
ne l'est pas. La mise en garde du cycle 385 reste donc **non levée** :
l'attribution « lots multi-quads = texte manquant » demeure déduite, non
vérifiée.

P1.3 non franchie. La première mission ne se joue pas.
`recompiler-generated` n'est pas `verified`.
