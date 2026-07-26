# AC6 cycle 246 — bind du descripteur texture par stage

## Identité et méthode

- target : `ac6-xbox360-pal` ;
- module : `default.xex` ;
- SHA-256 :
  `acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde` ;
- image base : `0x82000000` ;
- projet Ghidra canonique : `ace-combat-6`.

L'analyse est exclusivement headless, statique, `-readOnly -noanalysis`.
Aucun Xenia, VNC, GUI ou geste humain n'est utilisé.

## Première frontière Xenos qualifiée

Le slot vtable byte `+0x28`, commun aux trois classes
`TextureContext*Xenon`, arrive à `0x8234FDA0`. Son appel à `0x821E1088`
transmet exactement :

```text
r3 = *(caller_context + 0x04)  // objet device
r4 = index reçu par le slot
r5 = texture_context + 0x1C   // bloc descripteur fermé au cycle 245
r6 = 0x80000000 >> index      // bit dirty du stage
```

La routine `0x821E1088..0x821E1200` utilise `r4` deux fois comme index de
stage :

- `(r4 + 0xC3E) * 4` adresse la table de pointeurs texture du device ;
- `(r4 + 0x30) * 0x18` adresse une entrée de shadow state de 24 octets.

Lorsque `r5` est non nul, elle lit les six mots du descripteur aux offsets
`+0x1C`, `+0x20`, `+0x24`, `+0x28`, `+0x2C` et `+0x30`, les normalise avec
l'état courant du device, puis écrit exactement six mots dans l'entrée de
24 octets. Elle ajoute ensuite `r6` au masque dirty à `device+0x18` et stocke
le pointeur `r5` dans la table indexée par stage. Le pointeur précédemment lié
est conservé dans `r27` et passe par le chemin de retrait différé lorsqu'il
est remplacé.

Cela qualifie la première arête runtime suivante :

```text
MATE texture key
  -> TextureContext*Xenon
  -> descriptor at object+0x1C
  -> per-stage texture/fetch shadow state in 0x821E1088
```

Le rôle de bind texture par stage est `confirmed` structurellement. Le nom
XDK exact reste `cross-match`, car aucun symbole retail ne le fournit.

## Défaut de frontière dans la référence AC6Recomp

Le checkout de référence `.tools/ac6-recomp-reference` est au commit
`c5b089fb6988ac504ba394db611543bda2fb2c96`. Sa configuration publie
`0x821E10C8` comme entrée `rex_sub_821E10C8`, et `src/d3d_hooks.cpp` la nomme
`D3DDevice_SetTextureFetchConstant` avec le contrat `r4=Register`.

Dans le XEX PAL qualifié, `0x821E10C8` n'est pas une entrée de fonction :
c'est l'instruction `lwz r26,0x1c(r5)` située au milieu de la routine entrée à
`0x821E1088`. Plus important, `0x821E10B8` a déjà exécuté
`add r4,r31,r4`. Au point du hook, `r4` ne contient donc plus l'index de stage,
mais `device + stage`.

La capture actuelle :

```cpp
uint32_t reg = ctx.r4.u32;
if (reg < ac6::d3d::kMaxFetchConstants) {
    g_shadow.texture_fetch_ptrs[reg] = ctx.r5.u32;
}
```

ne peut pas produire un index de registre valide pour ce build. Le hook peut
rester utile comme mid-hook d'observation, mais son contrat et son compteur de
stage sont incorrects tant qu'ils ne reconstruisent pas l'index original ou
ne sont pas déplacés à l'entrée `0x821E1088` dans une configuration qui
recompile cette vraie frontière.

Aucun patch n'est appliqué à la référence externe dans ce cycle : il faut
d'abord vérifier le découpage XenonRecomp et les conséquences d'un changement
d'entrée sur le code généré. Le constat est toutefois une frontière de
correction précise, pas une hypothèse visuelle.

## Limites restantes

Cette fermeture ne qualifie toujours pas :

- la technique, la passe ou la permutation shader issue du MATE ;
- le sampler state associé au même stage ;
- la dominance jusqu'au draw backend ;
- le typedef XDK exact des six mots du fetch descriptor.

Le verdict global `NO_QUALIFIED_MATERIAL_BIND` reste donc valide pour la
jointure complète MATE -> permutation -> draw, mais il ne doit plus être
interprété comme une absence de bind texture : le sous-chemin texture par
stage est maintenant fermé.

## Preuves et validation

- `artifacts/ac6-cycle246-texture-fetch-bind.log` ;
- `artifacts/ac6-cycle246-texture-fetch-function-full.log` ;
- `artifacts/ac6-cycle246-texture-context-bind-range.log` ;
- `artifacts/ac6-cycle246-texture-descriptor-bind-validation.log` ;
- `workspaces/ace-combat-6/scripts/VerifyTextureDescriptorBind.java`.

Le vérificateur réexécutable contrôle 22 instructions PPC critiques : passage
de `object+0x1C`, indexation par stage, six lectures du descripteur, six
écritures du shadow state, dirty bit, publication du pointeur et mutation de
`r4` avant `0x821E10C8`. La passe headless ne contient aucune erreur de script
et les 22 assertions sont présentes.

## Prochaine frontière

Corriger d'abord la carte d'instrumentation AC6Recomp autour de
`0x821E1088/0x821E10C8` dans un patch borné et testé. Pour poursuivre la
reconstruction statique, suivre séparément les slots byte `+0x2C` vers les
sampler states et la sélection shader/permutation ; ne plus rescanner le bind
texture déjà fermé et ne pas demander de session humaine.

Le cycle 247 applique le correctif minimal sans déplacer le chunk : l'index de
stage est reconstruit par `uint32(r4-r3)`, puis vérifié sur les 32 stages et le
wraparound. La prochaine frontière devient donc le contrat sampler du slot
byte `+0x2C`.
