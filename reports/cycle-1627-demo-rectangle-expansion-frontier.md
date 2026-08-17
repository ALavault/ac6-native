# Cycle 1627 — expansion Vulkan du rectangle neutral

Date : 2026-08-15  
Cible : démo Xbox LIVE PAL `Default.xex`  
SHA-256 : `de917873f601e2a2208d75ab907e918ce941a42378d0d088705ecb4477405da8`

## Résultat

Le premier draw normal Vulkan atteint bien le pipeline et écrit exactement la
moitié du rectangle attendu : 115 000 pixels noirs et 115 000 pixels sentinelle
sur le resolve test-only 640×360. Ce résultat invalide l'emploi direct du shader
vertex ReXGlue par défaut avec trois sommets comme émulation d'un
`RectangleList` Xenos.

La variante générique ReXGlue
`Shader::HostVertexShaderType::kRectangleListAsTriangleStrip` a ensuite été
testée sur les deux microcodes vertex PAL exacts :

| Microcode PAL | Taille | Résultat `spirv-val` Vulkan 1.1 |
|---|---:|---|
| `93488cb9…402b` | 27 dwords | rejet, `OpBranch` hors bloc, SPIR-V `2fbf4793…8fcf` |
| `586168ec…3cc0` | 15 dwords | rejet, `OpBranch` hors bloc, SPIR-V `f872463b…fadb` |

Le runtime demeure fail-closed : aucun de ces modules invalides n'est scellé,
conservé ou soumis. La sélection rectangle expérimentale a été retirée.

Le chemin Vulkan générique réellement préféré par ReXGlue lorsque
`geometryShader` est disponible a ensuite été adapté à l'interface atteinte :
le vertex shader reste inchangé, un geometry shader reçoit le triangle source,
choisit son côté le plus long avec les comparaisons strictes ReXGlue, réordonne
les trois sommets et synthétise le quatrième par `-v0 + v1 + v2`. L'interface
atteinte ne contient que `SV_Position`, sans interpolateur ni clip distance.

Le source générique est compilé exclusivement sous le build par DXC épinglé,
validé par `spirv-val` épinglé, puis converti en tableau C++ généré sous le
build. Le SPIR-V reproductible mesure 1 900 octets et porte le SHA-256
`c3d714958d98c241f752a0335ec7de212d80e422ee0a311f7e4020942c3cd584`.
Ni SPIR-V généré, ni microcode, ni désassemblage n'est suivi.

## Provenance générique

- ReXGlue épinglé : `cb58065c793429aa92895d778af58d12e9d26d8f`.
- Sélection : `SpirvShaderTranslator::GetDefaultVertexShaderModification`.
- Variante : `Shader::HostVertexShaderType::kRectangleListAsTriangleStrip`.
- Chemin Vulkan normal ReXGlue : lorsque `geometryShader` est disponible, le
  `PrimitiveProcessor` conserve le vertex shader `kVertex` et
  `VulkanPipelineCache::GetGeometryShader` synthétise le quatrième sommet du
  rectangle depuis le triangle source. ReXGlue/Xenia restent une autorité
  Xenos générique, pas une preuve AC6.

## Provenance IB PAL conservée

- premier dword final : fonction `0x821B0D20`, store `0x821B0D70`, bytes
  `95 4B 00 04` ;
- dernier dword : fonction `0x821B9F70`, store `0x821BA01C`, bytes
  `94 CA 00 04` ;
- publication ring : fonction `0x821B9BC8`, store `0x821B9D24`, bytes
  `7D 2A C1 2E`.

Ces faits proviennent des watchpoints inverses `rr`, des lignes XenonRecomp
littérales et des bytes PAL qualifiés. Ils ne changent pas avec l'échec de la
variante shader.

## Validation

- deux replays neutral Vulkan depuis stores neufs : RTPLY
  `c5357c6d9639c2a675161bd10c3cdbe97096df0dac11d760fe8a2d964b1c5794`
  et rapport
  `33b6c8b3759461fd7720ca3942258cee3fcd1ec4732746a1df8314e50b5685a7`,
  byte-identiques 2/2 ;
- le readback 640×360 contient 230 000 pixels noirs, zéro sentinelle et le
  digest oracle
  `0b150fd32588b1daca5569992ebe559c0102c837306b1af4c44d35128ec58366` ;
- CTest codegen OFF : 18/18 ; codegen ON : 17/17 ;
- audits source et complexité : PASS ; `src/main.cpp` réduit à 1 177 lignes ;
- aucun SPIR-V, microcode, désassemblage ou actif propriétaire ajouté au projet.

## Prochain checkpoint

Soumettre le second rectangle copy avec ses constantes déjà qualifiées, puis
transférer son résultat vers l'EDRAM test-only et le comparer à l'oracle CPU.
Le compute resolve et le readback 1280×720 restent interdits tant que cette
jointure draw normal → EDRAM → copy n'est pas exacte. START reste gelé.
