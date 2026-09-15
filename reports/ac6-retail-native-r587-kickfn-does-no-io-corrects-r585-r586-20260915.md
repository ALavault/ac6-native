# r587 — Preuve runtime : le spin `0x82870f48` ne fait aucune I/O (r585 et r586 corrigés)

## Qualification

- **Ghidra project** : `ghidra-projects/ace-combat-6`, `default.xex` (retail
  `ntsc-uj`). Assemblage lu dans `exports/82345c88.json`.
- **Oracle** : non. Aucun budget N3.
- **Preuve runtime** : deux captures courtes du binaire natif
  (`AC6_NATIVE_THREAD_SAMPLE=1 AC6_NATIVE_NTREADFILE_ERR=1`), l'une pré-mode
  (~192 k appels), l'autre bootée jusqu'à TitleMovie (18,7 M appels). Le spin
  `kickfn` étant lié au CPU et présent dès `ms=100`, ces captures ne dépendent
  **pas** du GPU — corrige la prémisse de r584/r585/r586 selon laquelle
  diagnostiquer exigeait un run d'1 h jusqu'à MissionTitle.

## Ce que la capture établit (avec preuve)

Instrumentation ajoutée (scaffolding diagnostic local, gated env) : le hook
`sub_82345C88` logue désormais les champs de la requête (`r587 kickfn-fields`),
et `NtReadFile` logue les handles invalides sur son propre gate borné
(`AC6_NATIVE_NTREADFILE_ERR`, 64 premiers).

1. **`r3` EST `param_1`, `+0x18` EST le champ dispatcher** — vérifié sur
   l'assemblage, pas déduit :
   ```
   82345c9c  or r31,r3,r3          ; param_1 <- r3
   82345ca0  lwz r4,0x18(r31)      ; iVar2 = *(param_1+0x18)
   82345cac  lwz r11,0x4(r4)       ; *(iVar2+4)  (génération réelle)
   82345cb0  lwz r10,0x14(r31)     ; *(param_1+0x14) (génération attendue)
   82345cb4  cmpw cr6,r11,r10 ; bne -> 82345d54 (chemin erreur)
   82345cbc  lwz r11,0x10(r4)      ; *(iVar2+0x10) (device busy)
   ```

2. **`disp = *(0x82870f48+0x18) = 0x119` en permanence** — 49 échantillons
   jusqu'à l'appel 18 776 000, y compris dans le mode TitleMovie. Jamais un
   pointeur guest valide (`0x82xxxxxx`). `gen_act = *(disp+4) = *(0x11d) = 0` ;
   `busy=0` ; `device = *(disp+0x14) = 0`. `gen_exp = *(param_1+0x14)` varie
   (0x4035, 0x7e7f, 0x20aab, …) : le poll rafraîchit une génération attendue,
   mais la cible (`disp`) n'est jamais peuplée → garde de génération/absence de
   device → **le chemin de lecture n'est jamais pris**.

3. **Zéro I/O fichier** — **0** ligne `[NtReadFile] invalid` sur 18,7 M appels.
   `read_file` renvoie `false` (INVALID_HANDLE) sur tout handle inconnu ; aucune
   n'est apparue. Négatif borné : ce spin n'atteint jamais `NtReadFile`.

## Corrections

- **r586 (handle PAC `0x829xxxxx` rejeté par `read_file`) : FAUX pour cet
  objet.** Le spin `0x82870f48` ne fait aucune I/O fichier (§3) ; le handle PAC
  n'est donc pas la cause de ce spin. r586 avait relié r585 à r240/r241 sur une
  hypothèse ; la preuve runtime l'infirme.
- **r585 (pompe de lecture asynchrone par morceaux qui ne se termine pas) :
  mécanisme FAUX pour l'objet observé.** `disp=0x119` (pas de device) : la
  branche de lecture par morceaux n'est jamais exécutée ; le spin est le
  retour d'erreur instantané (garde génération/device), pas une lecture.
- **r497 (déjà corrigé en r585) reste corrigé** : `0x82345CE0` n'est pas une
  fonction ; `sub_82345C88` lit `obj+4`, ne le signale pas.

## Red herring : argument fort, non prouvé à 100 %

`0x82870f48` est un **objet global statique** (page data image), tandis que les
nœuds de l'arbre de chargement sont alloués dynamiquement via
`Function_821D2FC0(0x829e6218, key)`. Un global statique n'est donc, par
structure, très probablement **pas** un nœud parcouru par le poll de chargement
`Function_821D2860`. Il spinne dès `ms=100` et le boot progresse à travers les
modes malgré lui. → Ce spin est **très probablement un red herring** pour le gel
MissionTitle.

**Non prouvé** : que `0x82870f48` soit totalement étranger au gel. Le poll de
chargement du gestionnaire de mode n'est armé qu'à MissionTitle (r584
`load_calls=1`) ; en théorie un objet toujours en échec ne deviendrait bloquant
que là. Trancher exige d'instrumenter le poll lui-même à MissionTitle (voir
ci-dessous). Ce que la nature « global statique, hors arbre de chargement »
rend peu probable.

Ce que `0x82870f48` **est** reste non identifié : l'adresse n'apparaît dans
aucun corps de fonction (accès base+offset), et `sub_82345C88` n'a aucun
appelant statique (`callers=[]`, dispatché par pointeur runtime). Candidats
plausibles non confirmés : poll de disponibilité d'un sous-système (DVD, audio
XMA, périphérique) jamais initialisé côté HLE.

## Prochaine barrière (précise, et ce qu'elle coûte)

La vraie cible n'est pas `0x82870f48` mais le **nœud dont la `step` (vtable+0x14)
renvoie 0** dans le poll `Function_821D2860` à MissionTitle. L'instrumenter :
hooker `Function_821B8430`/`Function_821D2860` pour logger, à MissionTitle,
chaque nœud visité et le résultat de sa `step`/`query`. **Coût** : atteindre
MissionTitle, soit ~1 h sous saturation GPU (`neural_amp`), car cette
instrumentation-là n'est pas observable en pré-boot. C'est le seul point de ce
cycle qui reste bloqué par le GPU.

## Décisions prises

1. **Corriger r585 et r586 sur preuve runtime** plutôt que perpétuer un
   mécanisme faux (discipline : « corriger ses prédécesseurs et soi-même »).
2. **Ne pas déclarer `0x82870f48` red herring sans réserve** : l'argument
   structurel est fort mais la preuve définitive exige le hook du poll à
   MissionTitle. Formulé comme « très probablement », pas « établi ».
3. **Garder l'instrumentation** (`r587 kickfn-fields`, `NtReadFile err` borné) :
   elle vient de démontrer sa valeur et sert la prochaine étape ; gated env,
   coût nul quand désactivée.
