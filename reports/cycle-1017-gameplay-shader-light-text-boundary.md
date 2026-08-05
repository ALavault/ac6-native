# Cycle 1017 — gameplay shaders, lumière et texte au bord du monde noir

Date: 2026-08-05

## Provenance et lane

- Classification: `bridge`; aucune preuve `stock/observe` ou native n’est
  déduite de ce run.
- Projet Ghidra canonique: `ghidra-projects/ace-combat-6`.
- PAL `default.xex` SHA-256:
  `acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde`.
- ELF bridge SHA-256:
  `896b0b79608235d9063231fc472828d8b1c2545438c8dfe5195702174922d055`.
- Source bridge: commit
  `b8b03c7a89dc7f23bcd7844d15aa5080d480bf11`, worktree externe dirty.
- Log complet: `/tmp/ac6-cycle1017-d5b4-manual/ac6recomp.log`, SHA-256
  `ec79671077b297b58f9f30a3178a4f2e9902832688b2174af1f0b5ac7e1a4da1`.
- Configuration: `SDL_AUDIODRIVER=dummy`, Xvfb `:110`, 1280x720,
  `--log_max_file_size_mb=1000`, capture/render catalog activés.

Le flux `ac6recomp-follow.log` du harness est resté vide malgré un log runtime
valide. Le parcours a donc été piloté par un contrôleur externe state-driven,
avec les mêmes états et entrées; ce défaut d’observabilité n’est pas classé
comme défaut guest/rendu.

## Frame gameplay qualifiée

Le parcours atteint `type28=30 → 37 → 35`, `selector44=3 → 10`,
`type28=6 → 8 → 10`, puis campagne, briefing, loadout, cinématique et
`flight-hud-baseline`. La fenêtre gameplay instrumentée est:

- `trigger_frame=26536`, `capture_frame=26537`,
  `[ac6-gameplay-frame-complete] draws_logged=1325`;
- 29 624 `PRESENT`, 69 265 binds et 1 679 resolves sur le run;
- 29 288 diagnostics de régions resolve vides sur le run; huit sont dans la
  fenêtre frame 26537, avec des intervalles scissor `0..0` ou `640..640`;
- présentation: `guest_swap_texture`, 1280x720→1280x720, `scaled=no`,
  vertex/pixel swap `72CBCAA6A7984111` / `2E372EA28CC404B7`.

La capture HUD montre la géométrie verte attendue sur fond noir. Le monde et
l’avion ne deviennent pas visibles. W/S/A atteignent néanmoins l’entrée
canonique (`ly=0x7FFF`, `ly=0x8001`, `lx=0x8001`) puis reviennent à zéro.
Les captures de vol ont les hashes suivants: HUD baseline
`7aed2448b77fbfca5ffc73bd6bab016d6a0a5b3cc9d55548f5a15e95efed5a22`, puis
pitch/roll/yaw/throttle/brake
`e8f39f8da62f4650173965e94e68cdde68f21198eea1483a902f5a0382f0b1ec`.
Cela qualifie l’absence de réponse visible, sans confondre entrée reçue et
mouvement de l’avion.

## Shader et lumière

Le dump a produit 532 shaders. Les fragments retenus sont:

- `shader_D5B4F4A878949938.ucode.frag`, SHA-256
  `e30a417e77071d9b42779afa61849fbdd9d94a6c086e3e2e368409d8f047f5bf`;
- `shader_72D1B1643EB3FB9A.ucode.frag`, shader cube/multi-texture;
- `shader_91316876F6AE74B3.ucode.frag`, texture puis modulation alpha/couleur;
- `shader_8F1C48BA92C8E43E.ucode.frag`, SHA-256
  `2e4a3dd67e2eb23ebf9863f7b8b956bc154f8d79e3551dd790385d5e450e98d7`.

D5B4 effectue bien `tfetch2D r3 ... tf0`, une chaîne de modulation/lumière,
saturation et termine par `max oC0`; ce n’est ni un shader vide ni un dump
malformé. Sur les draws map object Mission 01, les constantes observées sont
stables et plausibles:

```
c129=3EC6C6C8,3EEEEEF0,3F0B8B8C,B912CCF7
c138=3F800000,3F800000,40400000,3F000000
c140=3F70F0F2,3F6EEEF0,3F2CACAD,3F800000
c142=3F800000,40A00000,40400000,3F800000
c254=00000000,3E800000,BF800000,3FB8AA3B
c255=3F000000,3D20A0A1,3F70F0F1,3F800000
```

Le frame contient 372 draws D5B4 sur `surface=0A010280`. Les entrées
`mapobj_m01_l_brg2_n`/`mapobj_m01_l_brg1_n` utilisent notamment les textures
format 20, 256x256, aux bases `045FB000` et `045A2000`; leurs images hôte sont
créées avec les hashes runtime `8338381B94340E1C` et `AFF68DE151FA2C1B`.
Ce dernier fait ne prouve pas encore que les texels décodés sont non nuls.

Conclusion: l’upload des constantes de lumière et la traduction statique de
D5B4 ne sont pas le premier défaut qualifié. Les deux candidats restent
l’échantillonnage/décodage `tf0` de ces textures map object et la frontière
surface→resolve/presentation. Les huit régions resolve vides sont une piste,
pas une autorisation de modifier le resolve sans A/B ciblée.

## Texte et HUD

Le shader `8F1C` est également valide:

```
tfetch2D r0, r0.xy, tf0
mul oC0, r0, r1
```

Les draws UI du frame utilisent `surface=14000500`, `VS=472913F460D4B446`,
`PS=8F1C48BA92C8E43E` et une texture `02F70000`, format 6, 512x512, avec
`xxh3=D377029FA14975F2`. Les glyph sheets connues `028B7000` et `03514000`
ont bien des sources non nulles (`160/256` et `175/256` octets non nuls), mais
ne sont pas bindées dans ce frame de vol. L’absence de texte en vol ne permet
donc pas d’attribuer le monde noir au shader texte ni de réutiliser directement
le défaut glyph DXT4/5 des dialogues.

## Suite autorisée

1. Rejouer la même route avec `ac6_d5b4_texture_sample_override=1` puis `2`,
   et comparer la capture frame 26537; une absence de delta rejettera le
   sample D5B4 comme cause du monde noir.
2. Ajouter une sonde bornée des octets source et du contenu hôte pour
   `045A2000`/`045FB000`, puis une lecture statistique du target avant/après
   resolve.
3. Ne corriger que le contrat dont cette A/B identifie la rupture. La lane
   `stock/observe` et la parité retail restent ouvertes.

Aucun PAC retail complet n’a été copié ou téléversé; les dumps et captures
restent des artefacts bornés sous `/tmp` référencés par hash.
