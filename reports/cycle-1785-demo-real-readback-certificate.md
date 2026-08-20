# AC6 PAL démo — readback réel certifié, cycle 1785

Verdict : **COPY-READY / NORMAL-DRAW-BLACK**, `supported=false`.

Le resolve atteint n'impose plus que le readback normal soit noir. Il accepte
uniquement l'image RGBA8 réelle 640×360, la matérialise dans l'EDRAM atteint,
puis exige l'égalité CPU/Vulkan des échantillons EDRAM, pixels de copie et
padding avant tout writeback guest. Aucune image n'est fabriquée par ce chemin.

Un probe Vulkan froid, neutral, de dix ticks atteint un normal draw, un resolve
et un writeback guest. Son readback reste noir (`0b150fd3…8366`) et sa sortie
1280×720 reste noire (`0c660f2b…3a5f`) : ni frontend ni mission ne sont
promus. La prochaine frontière est donc la production du normal draw, pas la
copie, l'endian ou le tiling.

Le reçu exact est
[`ac6-demo-vulkan-real-readback-1785-v1.json`](../analysis/demo/ac6-demo-vulkan-real-readback-1785-v1.json).
