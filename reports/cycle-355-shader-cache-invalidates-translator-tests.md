# Cycle 355 — un cache de shaders sur disque invalide silencieusement toute expérience de traduction

## 1. Le piège, découvert en tentant le test décisif

Le test du cycle 354 : forcer l'attribut de sommet `FMT_8_8_8_8` à 1.0 dans le
traducteur SPIR-V, et voir si la couche d'interface réapparaît.

Première exécution : **rien ne change**, image identique à 111 Ko. Lecture
immédiate : « la couleur de sommet n'est pas la cause, hypothèse réfutée ».

Avant de l'écrire, vérification de la vivacité du canal — la règle apprise six
fois dans cette enquête. Résultat :

```
build-rt/cache/shaders/shareable/4E4D07D1.xsh
build-rt/cache/shaders/shareable/4E4D07D1.fbo.vk.xpso
```

**Les shaders traduits et les objets de pipeline Vulkan sont mis en cache sur
disque**, et rechargés au démarrage. Le traducteur modifié **ne s'exécutait
pas**. Le test ne testait rien.

Cache effacé, même binaire, même cvar : la capture passe de **111 Ko à 343 Ko**
et le contenu diffère complètement. La différence prouve que la traduction a
bien été refaite cette fois — donc que la première exécution ne l'avait pas
faite.

## 2. Portée du piège

Cela vaut pour **toute** expérience touchant :

- `spirv_translator*.cpp` (traduction des shaders Xenos) ;
- l'état de pipeline Vulkan.

Sans `rm -rf build-rt/cache/`, une telle modification est **silencieusement
ignorée**, et l'expérience rend « aucun effet » — indiscernable d'une hypothèse
réfutée. C'est le même motif que la capture branchée sur D3D (cycle 346) et que
les compteurs à zéro non alimentés (343-345, 351) : **un instrument débranché
rend un résultat qui ressemble à une mesure.**

## 3. Le test lui-même : non concluant

Après effacement du cache, l'exécution n'a **pas atteint l'écran de
sauvegarde** : la recompilation des shaders ralentit le démarrage, et les
frappes sont parties pendant la cinématique d'introduction. La capture montre la
cinématique, pas le dialogue.

**Le test reste donc à faire**, avec un temps de stabilisation plus long après
effacement du cache. L'hypothèse du cycle 353 — la couche est
« texture × couleur de sommet » et la couleur arrive à zéro — n'est **ni
confirmée ni réfutée**.

## 4. Protocole corrigé pour la reprise

```bash
rm -rf build-rt/cache                     # obligatoire
# stabilisation >= 60 s, la recompilation des shaders retarde le démarrage
./ac6recomp --ac6_performance_mode=false --mnk_mode=true \
            --ac6_force_white_vertex_colour=true
# puis Escape, space, space et capture
```

Si l'interface apparaît : la couleur de sommet est la cause, chemin
`vfetch FMT_8_8_8_8`. Sinon : la dépendance du cycle 353 n'est pas la cause.

Le correctif diagnostique est conservé :
`patches/rexglue-force-white-vertex-colour-diagnostic-20260731.patch`.

P1.3 non franchie. La première mission ne se joue pas.
`recompiler-generated` n'est pas `verified`.
