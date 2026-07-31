# Cycle 383 — la source du copieur n'est pas dans l'arbre ; la référence CPU l'est

## 1. Limite matérielle de l'enquête

Le copieur 128 bpb n'existe dans l'arbre que **compilé** :

```
src/graphics/shaders/vulkan_spirv/texture_load_128bpb_cs.h      (SPIR-V)
src/graphics/shaders/bytecode/d3d12_5_1/texture_load_128bpb_cs.h (DXBC)
```

Aucune source HLSL. Le calcul d'adressage du chemin réellement emprunté **ne
peut donc pas être relu ici**. C'est une limite de l'arbre, pas de la méthode, et
elle borne ce que ce cycle peut conclure.

## 2. La référence CPU, elle, est lisible

`pipeline/texture/conversion.cpp:88` porte le même calcul de pavage Xenos :

```c
static uint32_t TiledOffset2DRow(uint32_t y, uint32_t width, uint32_t log2_bpp) {
  uint32_t macro = ((y / 32) * (width / 32)) << (log2_bpp + 7);
  uint32_t micro = ((y & 6) << 2) << log2_bpp;
  return macro + ((micro & ~0xF) << 1) + (micro & 0xF)
       + ((y & 8) << (3 + log2_bpp)) + ((y & 1) << 4);
}
```

## 3. Une vérification immédiate, et négative

Le terme de macro-tuile divise par 32 : `width / 32`. Une division entière
tronquerait pour toute largeur non multiple de 32.

Or la largeur passée est `guest_pitch_aligned`, **toujours multiple de 32** par
la règle établie au cycle 382 :

```
03514000  pitch 64  -> 64/32 = 2    exact
028B7000  pitch 96  -> 96/32 = 3    exact
028E9000  pitch 256 -> 256/32 = 8   exact
02953000  pitch 64  -> 64/32 = 2    exact
028B2000  pitch 32  -> 32/32 = 1    exact
```

**Exacte pour les sept.** L'indexation par macro-tuile n'est donc pas en cause —
ni pour les fautives, ni pour les témoins. Vingt-six causes écartées.

## 4. Ce que cela laisse

Les termes restants du calcul sont les micro-décalages
(`y & 6`, `y & 8`, `y & 1`, `x & 7`, `y & 16`) — indépendants de la largeur et
identiques pour toutes les textures. Rien dans cette référence CPU ne distingue
les deux géométries fautives.

Deux conséquences honnêtes :

1. si le shader GPU implémente la même formule, l'adressage n'est pas la cause,
   et la contradiction se déplace encore ;
2. si le shader GPU diverge de cette référence, la divergence est invisible
   depuis cet arbre.

Trancher exige soit la source du shader (absente), soit une lecture arrière de
l'image hôte (non réalisée), soit une désassemblage du SPIR-V embarqué — la
seule voie restante entièrement contenue dans l'arbre.

P1.3 non franchie. La première mission ne se joue pas.
`recompiler-generated` n'est pas `verified`.
