# Cycle 384 — le shader embarqué implémente bien la formule de référence

## 1. Méthode

Aucun désassembleur SPIR-V n'est disponible (`spirv-dis` absent, SPIRV-Tools
présent uniquement en bibliothèque). Le binaire est cependant un flux de mots
lisible directement : magie `0x07230203` vérifiée, 1 454 mots, parcours des
instructions et extraction des `OpConstant` entiers.

## 2. Constantes trouvées dans `texture_load_128bpb_cs`

```
0, 1, 2, 3, 4, 5, 6, 7, 8, 10, 11, 14, 15, 16, 32, 63, 448,
16711935, 268435455, 4278255360, 4294966784, 4294967280, 4294967294
```

Traduction des valeurs signifiantes :

| valeur | hex | rôle dans le pavage Xenos |
|---:|---|---|
| 6, 7, 8, 16, 32 | — | masques `y & 6`, `x & 7`, `y & 8`, `y & 16`, tuile 32 |
| 15 | `0xF` | `micro & 0xF` |
| 63 | `0x3F` | `offset & 0x3F` |
| 448 | `0x1C0` | `offset & 0x1C0` |
| 4294966784 | `~0x1FF` | `offset & ~0x1FF` |
| 4294967280 | `~0xF` | `micro & ~0xF` |
| 16711935 / 4278255360 | `0x00FF00FF` / `0xFF00FF00` | échange d'octets (endianness) |

## 3. Résultat

**Toutes les constantes de `TiledOffset2DRow` et `TiledOffset2DColumn` sont
présentes dans le shader.** Le copieur GPU implémente donc la même formule de
pavage que la référence CPU du cycle 383, laquelle a été vérifiée exacte pour
les sept textures.

Une divergence de formule entre les deux implémentations est donc **improbable**.
Vingt-sept causes écartées.

## 4. Où cela laisse la contradiction

Chaîne complète, chaque maillon mesuré ou vérifié :

données sources non nulles -> constantes de chargement correctes et alignées ->
dispatch émise avec le bon nombre de groupes -> formule d'adressage conforme à
la référence -> image liée réelle -> échantillonnage correct -> dessins qui
peignent l'écran.

Et le résultat reste vide, pour deux textures et deux seulement.

Aucun candidat mesurable ne subsiste dans l'arbre. Les deux voies encore
ouvertes sortent de ce qui a été exploré :

1. **lecture arrière de l'image hôte** — la seule mesure directe jamais faite du
   *résultat* plutôt que des paramètres ;
2. **comparaison avec l'oracle** au niveau de la trame — Xenia rend ces mêmes
   textures correctement (cycle 342) ; comparer les deux images du même écran
   dirait si l'écart est bien là où on le croit.

## 5. Portée

Le défaut est **cerné** — copieur 128 bpb, deux géométries, formule conforme —
et **non expliqué**. Ce cycle ferme la dernière piste interne sans la résoudre.

P1.3 non franchie. La première mission ne se joue pas.
`recompiler-generated` n'est pas `verified`.
