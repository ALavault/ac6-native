# AC6 cycle 1540 — requalification de Xenia Canary natif Linux

Date : 2026-08-12.

## Verdict

Xenia Canary natif Linux ne remplace pas encore la route Wine comme oracle
visuel AC6. Le binaire Linux déjà installé, le commit officiel épinglé et le
head officiel testé créent tous une fenêtre et une swapchain Vulkan 1280x720,
chargent le XEX PAL puis lancent le module, mais aucune frame invitée visible
n'est produite sous Xvfb.

Un profil natif jetable est une condition de progression importante : sans
profil, le build épinglé affiche `Game requested exit to dashboard`; avec le
profil chargé en slot 0, AC6 crée ses fils, enregistre le client audio et bind
son socket, puis reste noir. Les captures du contenu à 8 et 20 secondes sont
pixel-identiques. Le head officiel du 12 août reproduit exactement cette
frontière.

La route qualifiée reste donc le build Windows `16e1eb8` sous Wine sur
l'affichage GNOME vivant. Le natif reste intéressant pour l'instrumentation,
mais pas comme validation visuelle ou gameplay à ce stade.

## Identité et confinement

- Cible : AC6 Xbox 360 PAL, title ID `4E4D07D1`.
- XEX : SHA-256
  `acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde`.
- Racine d'expérience : `/tmp/ac6-xenia-native-retest.T3ynnD`.
- Aucun byte retail n'a été copié dans les artefacts : les harness temporaires
  pointaient vers le XEX local par lien symbolique.
- Aucun profil ni save Wine n'a été lu, copié, renommé ou modifié. Le profil
  utilisé pour les deux essais longs natifs a été généré dans leur `HOME`
  temporaire, puis chargé en slot 0.
- Avant le premier run et après le dernier : aucun processus Xenia/Wine actif;
  `ac6-xenia-wine-gui.service` était `inactive/dead`, `MainPID=0`. Aucun
  service n'a été arrêté ou redémarré.

Versions testées :

| Variante | Révision exacte | SHA-256 du binaire Linux |
| --- | --- | --- |
| binaire natif installé | `02d2cb5cc4bfbf047a3ee136923c263ae11f8356` | `98559834c570d4be8ba5d532f000aadf8ea6cf4d495be34a02b7ae766134007c` |
| source officielle épinglée | `16e1eb8e28a2935b75c36707b585a4f5e174ad43` | `134187430ad32e482b0527e8de10c8ae6371c115bfdfe1f841322d3c6bd9f37a` |
| head officiel qualifié | `e31142bd79a35cfa330530ab8ac59e9592e991cc` | `69ce567dfc761fad8aeb3fff35206645b577d312e216a4c2fb2466a98141ca1c` |

Le head est 51 commits après `16e1eb8`; son commit terminal est
`[Linux/XSocket] Added error ID mapping for error 11.`. Il contient aussi les
correctifs Vulkan/GPU, kernel et audio officiels intermédiaires. Les deux
worktrees étaient détachés sur les commits ci-dessus, avec sous-modules
initialisés, et `git status --short` était vide après build.

## Construction reproductible

Les deux sources officielles ont été construites sans patch, dans deux
worktrees temporaires distincts :

```bash
CC=/usr/bin/clang \
CXX=/usr/bin/clang++ \
VULKAN_SDK="$TMP_ROOT/vulkan-sdk/1.4.357.1/x86_64" \
PATH="$VULKAN_SDK/bin:$PATH" \
python3 ./xenia-build.py build --config=release
```

- Vulkan SDK Linux `1.4.357.1`, archive SHA-256
  `4b41e3b30e8aedaa5dac7c136561ab463eb316a25a54e2c6245f2c299ea1fb85`.
- `16e1eb8` : 702/702, `Success`; log SHA-256
  `253333c9405411c52c177227e612790268c4fe8c539162a049d590def1fb15dd`.
- `e31142bd` : 702/702, `Success`; log SHA-256
  `f5f78c97f5ca4e34475958025c01ceddcc4c51c6064e0b43f276dcd97515ec52`.
- Les URLs de commit incorporées dans les deux ELF correspondent exactement
  aux révisions annoncées; `ldd` ne signale aucune dépendance manquante.

## Runs bornés

Le harness de référence a d'abord été exécuté sans modification :

```bash
SDL_AUDIODRIVER=dummy \
XENIA_AC6_CAPTURE_SECONDS='8 20' \
XENIA_AC6_UNGATED_MAX_SECONDS=20 \
scripts/run_xenia_ac6_oracle_baseline.sh \
  "$TMP_ROOT/baseline-dummy" :178

env -u SDL_AUDIODRIVER \
XENIA_AC6_CAPTURE_SECONDS='8 20' \
XENIA_AC6_UNGATED_MAX_SECONDS=20 \
scripts/run_xenia_ac6_oracle_baseline.sh \
  "$TMP_ROOT/baseline-host-audio" :179
```

Pour le binaire épinglé, une copie byte-identique du harness a été placée dans
une arborescence temporaire qui résout son chemin de binaire codé en dur vers
le nouveau build. Les deux scripts ont le SHA-256
`5d49bf6a89b1eb35d5ec93cee8148c1249fa8ebd3e78c40252d9e3d04d2afc39`.

Les runs avec profil jetable ont utilisé les variables et options suivantes :

```bash
HOME="$RUN/home" \
XDG_CONFIG_HOME="$RUN/home/.config" \
XDG_CACHE_HOME="$RUN/home/.cache" \
XDG_RUNTIME_DIR="$RUN/runtime" \
SDL_AUDIODRIVER=dummy DISPLAY=:189 \
xenia_canary \
  --gpu=vulkan \
  --ac6_ground_fix=true \
  --logged_profile_slot_0_xuid="$THROWAWAY_XUID" \
  /path/to/qualified/default.xex
```

Chaque run avait son propre Xvfb 1280x720, son propre `HOME`, deux captures à
8/20 secondes et une limite de 23 secondes. Le dernier run a été arrêté par
`timeout --signal=TERM --kill-after=5s`; Xenia n'ayant pas répondu à `TERM`, le
runner a renvoyé 137 après `KILL`. Il ne reste aucun processus. La condition
de sortie naturelle exigée pour déclarer un succès n'est donc pas satisfaite.

Un premier essai du head a été rejeté avant comparaison : le run pinned lancé
par cette expérience était encore vivant et occupait le port remappé 10999.
Ces deux processus appartenant à l'expérience ont été arrêtés, l'inventaire a
été refait, puis le head a été relancé seul dans `head-e31142bd-clean-profile`.
Seul ce rerun à froid est utilisé ci-dessous.

## Résultats

| Run | Dernier point atteint | Captures 8/20 s | Contenu invité |
| --- | --- | --- | --- |
| installé `02d2cb5`, dummy | module PAL, NVIDIA Vulkan 1.3, swapchain, client audio | même SHA `42fa11d06dad4ca1a1793a84882aa8a1fcb6eaeeb7373dbec443d2150e448a6b` | noir pur |
| installé `02d2cb5`, audio hôte | même lancement, échec création driver SDL | même SHA `42fa11d06dad4ca1a1793a84882aa8a1fcb6eaeeb7373dbec443d2150e448a6b` | noir pur |
| pinned `16e1eb8`, sans profil | module lancé puis demande de retour dashboard | même SHA `3cc89f215572e01c01973f15c783fc82ae89eeb1f7a786157f9a3c39bb82ad4b` | dialogue Xenia, aucune frame de jeu |
| pinned `16e1eb8`, profil jetable | profil chargé, 29 fils, audio enregistré, port 999 remappé vers 10999 | même SHA `1a2eed61b14c9a9bd151f6226ceee6aad3a8fcbdb23dd2d405c1cd71ea3c01a1` | noir pur |
| head `e31142bd`, profil jetable, run à froid | mêmes étapes, bind sans erreur | même SHA `1a2eed61b14c9a9bd151f6226ceee6aad3a8fcbdb23dd2d405c1cd71ea3c01a1` | noir pur |

Le crop 1280x690 sous la barre de menu a moyenne `0`, écart-type `0` et une
seule couleur pour l'installé, le pinned avec profil et le head avec profil.
Le pinned et le head produisent exactement le même PNG, pas seulement des
statistiques proches. Le dialogue du run sans profil est de l'UI hôte et ne
constitue pas une frame invitée.

Hashes des journaux :

| Run | SHA-256 `xenia.log` |
| --- | --- |
| installé, dummy | `9e28f070387eb0e5fcd6f3352da3be9a186305c5a962643c1ac38631d35e4c06` |
| installé, audio hôte | `c3e7d6044d8314e792808773e02e8ac45a558a773493e57c9a0ac3c77605d601` |
| pinned, sans profil | `451548a16cbc6d365b8e493e954bb90df213b2aaf16f8559188b2fe2b10047d8` |
| pinned, profil jetable | `5a4905eee2f470fa41933b0f1e3acbeaa47a9834b0ac6761f69b7ee592ba67cc` |
| head, profil jetable, run à froid | `9556fab3470047e45ff1a5c92c7838b27363ec212332ea896c2637e740049621` |

Les logs qualifient pour les runs longs :

```text
ProfileManager: Found 1 Profiles
Loaded NativeProbe (...) to slot 0
Vulkan device 'NVIDIA RTX PRO 4000 Blackwell': API 1.4.329 (1.3 used)
VulkanPresenter: Created 1280x720 swapchain ...
Loading module GAME:\default.xex
Title name: ACE COMBAT 6
KernelState: Launching module...
AudioSystem::RegisterClient: client 0 registered successfully
XSocket::Bind: port 999 requires privileges, remapping to port 10999
```

Aucun `PRESENT` de jeu ni autre preuve visuelle invitée n'apparaît avant la
limite. Les logs s'arrêtent après la création des derniers fils invités.

## A/B audio

`SDL_AUDIODRIVER=dummy` est confirmé nécessaire sur cet hôte :

- avec `dummy`, le driver audio est créé et le client 0 est enregistré;
- sans la variable, ALSA échoue sur `cards.pcm.surround51`, puis
  `SDL_OpenAudioDevice() failed` et `CreateDriver failed for index=0`;
- les deux captures restent néanmoins identiques et noires. Le dummy ferme la
  frontière audio, pas la frontière de rendu.

## Comparaison Wine et frontière restante

La route Windows/Wine qualifiée utilise également `16e1eb8`, Vulkan, le fix
AC6 et un profil connecté. Son exécutable a le SHA-256
`c52d27f9a115c036257efbedd91006e74964e0c12aebb09b0c1dd93a31280f9a`.
Le handoff existant consigne une startup rendue jusqu'à l'écran titre et une
jouabilité confirmée par l'utilisateur. Le fichier `xconfig.settings` frais du
build Linux pinned est byte-identique à celui de la route Wine : taille 6680,
SHA-256
`447f0c9e11151746f76edb353cf38d61a77cc9b1054808247100ef84a15c5117`.
L'écart n'est donc pas expliqué par XConfig.

Première frontière observable restante : après lancement invité, création des
fils, audio et bind réseau, mais avant le premier contenu visible/PRESENT. Une
instrumentation native future doit charger un profil natif jetable et placer
ses sondes entre le bind et le premier `VdSwap`/submit GPU; sans profil, elle
mesurerait seulement le chemin de sortie dashboard.

Risque résiduel important : le Wine qualifié tourne sur l'affichage GNOME
vivant alors que cette requalification native est volontairement bornée sous
Xvfb. Elle démontre que le natif headless ne peut pas remplacer Wine; elle ne
sépare pas encore un défaut Linux d'un défaut propre à Xvfb. Un A/B natif sur
l'affichage vivant demanderait une session interactive explicitement autorisée
et une capture bornée, sans lancer Wine en parallèle.
