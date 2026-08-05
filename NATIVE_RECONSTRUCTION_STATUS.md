# Ace Combat 6 native reconstruction status

Updated: 2026-08-05 (Europe/Paris)

## Cycle 986 — décodage des payloads missions 3–5

Le nouvel extracteur `tools/extract_ac6_pac.py` lit uniquement les plages
sélectionnées de `DATA00.PAC`/`DATA01.PAC`; il ne charge ni ne copie un PAC
complet. Avec `ac6_mode1_codec.py`, il applique la clé pi/XOR indexée par
`DATA.TBL`, puis le raw DEFLATE PAL, et `ac6_fhm.py` valide le conteneur
résultant.

Les entrées physiques 11, 12 et 13 (missions 3, 4 et 5) se décodent toutes en
`FHM ` avec 26 enfants top-level, 9 FHM imbriqués et 112 lignes récursives,
sans échec de parse. Le catalogue conserve pour chacune la plage PAC exacte,
la taille stockée/décompressée, les hashes stockés/décodés et le codec. Les
missions 3–5 restent `partial` pour `payload_dependency_inventory`; les
missions 6–15 restent `not_attempted`.

Les mêmes identités de payload sont maintenant explicites pour les missions 1
et 2. Aucun payload décodé ni PAC complet n’est ajouté au dépôt. Build, CTest
sous Xvfb/audio dummy et audits campagne/code passent.

## Cycle 985 — routes physiques DPL→DATA.TBL 9–23

La preuve de la chaîne d’archive est maintenant reflétée dans le catalogue :
`0x821D1128` prend la branche directe pour tout identifiant DPL inférieur à
`0x39D`, transmet l’identifiant inchangé à `0x821CD130`, puis la table chargée
par `0x821CC250` résout l’entrée physique. Comme les sélecteurs 1–15 donnent
les DPL 9–23, leurs entrées physiques `DATA.TBL` sont désormais cataloguées
exactement 9–23.

La borne `0x39D` et le nombre de 926 entrées sont conservés avec les hashes
PAL, le projet Ghidra canonique `ace-combat-6`, la cible
`PAL-default-xex` et le module `default.xex`. Les missions 3–15 restent
`partial` uniquement pour `payload_not_decoded`; aucune ressource ni manifeste
natif n’est généré à partir de cette qualification physique seule.

Build, CTest sous Xvfb/audio dummy, audits campagne/code et générateur de
manifeste passent.

## Cycle 984 — correspondance campagne selector→DPL

Le catalogue machine-readable qualifie maintenant, pour le mode campagne 1,
la table XEX de `0x82065840` consommée par `0x821B6E58` : les sélecteurs 1 à
15 donnent respectivement les ressources DPL 9 à 23. La preuve est rattachée
au projet Ghidra canonique `ace-combat-6`, à la cible `PAL-default-xex` et au
SHA-256 du `default.xex` PAL.

Cette qualification ne déduit aucun index physique `DATA.TBL`. Les missions
3–15 sont donc `partial`, avec la frontière explicite
`DPL_to_DATA.TBL_route` et payload non décodé; seules les missions 1 et 2
conservent une route physique cataloguée, et la mission 2 reste non qualifiée
pour l’exécution interactive. L’auditeur refuse désormais toute table
selector→DPL absente, modifiée ou incomplète, ainsi que toute couverture de
catalogue divergente.

Le générateur de manifeste natif reste limité aux routes `qualified` : aucune
de ces correspondances partielles ne devient une route de runtime par
inférence.

## Cycle 983 — identité des ressources dans les checkpoints

Les checkpoints capturent maintenant les `AssetRecord` triés de la mission
(ID, chemin relatif et hash), et `MissionExecution::restore_checkpoint` les
compare au manifeste actuellement chargé avant toute mutation. Un chemin ou
hash modifié est refusé ; la capture est publiée atomiquement seulement après
la résolution de toutes les ressources.

`AC6SESS` passe en version 6 pour sérialiser ces identités. Les versions 1 à 5
restent lisibles ; leurs anciens checkpoints sans identité suivent le chemin
de compatibilité historique, tandis que toute nouvelle capture porte les
ressources qualifiées.

Build, CTest (`5/5`) sous Xvfb/audio dummy, smoke Vulkan et audits
campagne/code passent.

## Cycle 982 — sauvegarde disque d’une mission active

La reprise ne se limite plus à un objet mémoire : une Mission 1 active avec
loadout, objectif 1 complété et vol en cours est écrite dans `AC6SESS`, relue,
puis restaurée dans une nouvelle `MissionExecution`. L’objectif conserve son
état, la campagne reste `Active` avec son masque, et le joueur peut verrouiller
la cible puis tirer après reload.

Le contrat de checkpoint refuse désormais un joueur absent de la liste des
unités combat, à la fois dans `MissionExecution::restore_checkpoint` et dans
le validateur du codec session. Les checkpoints corrompus ne peuvent donc pas
publier un état HSM dont le joueur n’a pas de représentation combat.

Build, CTest (`5/5`) sous Xvfb/audio dummy, smoke Vulkan et audits
campagne/code passent.

## Cycle 981 — reprise d’un checkpoint de combat armé

Un checkpoint pris après résolution d’un projectile restaure désormais la
mission sans perdre les templates d’armes publiés au lancement. Le test
modifie la santé de la cible, restaure le checkpoint, verrouille puis retire
une seconde salve et vérifie la destruction.

La qualification a aussi fermé un défaut de cadence : une entrée de 250 ms
pouvait conserver plus d’une frame d’accumulateur parce que le plafond était
de 8 steps, puis être refusée par le validateur de snapshot. Le plafond passe
à 16 steps, suffisant pour la fenêtre d’entrée maximale de 250 ms ;
l’accumulateur reste borné à zéro côté flottant et compatible avec les
validateurs de sauvegarde.

Build, CTest (`5/5`) sous Xvfb/audio dummy, smoke Vulkan et audits
campagne/code passent.

## Cycle 980 — IA tolérante aux cibles inactives

Une règle IA dont la source ou la cible est déjà inactive est maintenant
ignorée sans invalider la frame. Cela permet aux vagues/despawn et aux dégâts
de progresser sans transformer une cible détruite en erreur permanente.
Le test de non-régression passe avec la suite complète.

## Cycle 979 — directeur IA déterministe

`MissionAiDirector` ajoute des règles génériques périodiques
`mission/tick/entity/target/weapon`, sans branche par mission. À chaque tick
éligible, il verrouille la cible et tente le tir via `CombatWorld`; les unités,
armes et dégâts restent donc sur le même chemin que le joueur. Les règles
dupliquées sont refusées.

Build, CTest (`5/5`) sous Xvfb/audio dummy, smoke Vulkan et audits campagne/code
passent.

## Cycle 978 — armes dans le launch TSV

Le quatrième champ optionnel du launch TSV accepte maintenant les templates
d’armes `id:damage:projectile_speed:cooldown:max_range`. Le format trois champs
historique reste valide ; les définitions sont parsées, bornées et validées
avant publication. Le test charge une arme depuis le TSV et vérifie le tir
issu de l’exécution mission.

Build, CTest (`5/5`) sous Xvfb/audio dummy, smoke Vulkan et audits campagne/code
passent.

## Cycle 977 — publication des armes au lancement

`MissionLaunchDefinition` porte désormais des `WeaponDefinition` validées.
`MissionExecution::launch` publie ces templates dans `CombatWorld` avant de
déclarer l’exécution lancée ; les IDs dupliqués ou paramètres invalides sont
refusés. Le test verrouille une cible, tire depuis l’exécution mission et
vérifie collision puis dégâts.

Build, CTest (`5/5`) sous Xvfb/audio dummy, smoke Vulkan et audits campagne/code
passent.

## Cycle 976 — génération fail-closed du manifeste campagne

`tools/generate_campaign_manifest.py` prend le catalogue campagne comme source
des routes et un fichier séparé pour les définitions gameplay (objectifs et
prérequis). Il n’émet que les entrées `qualified`, hash le catalogue dans le
TSV produit et refuse toute définition sans route qualifiée. Le run PAL produit
une seule ligne `Mission 1: selector 1 → DPL 9 → DATA.TBL 9`; les missions
partial/unqualified ne sont pas promues.

Build, CTest (`5/5`) sous Xvfb/audio dummy, smoke Vulkan et audits campagne/code
passent.

## Cycle 975 — replay déterministe de l’exécution mission

`MissionExecution::run_replay` rejoue maintenant les `InputFrame` via le même
chemin que le vol interactif, y compris `buttons` et les transitions Pause /
Resume. Deux exécutions lancées avec les mêmes assets, mapping et replay
produisent le même tick et la même pose finale.

Build, CTest (`5/5`) sous Xvfb/audio dummy, smoke Vulkan et audits campagne/code
passent.

## Cycle 974 — validation fail-closed des snapshots campagne

Les snapshots campagne v2 refusent désormais les états `Locked`/`Available`,
les masques d’objectifs dépassant le nombre déclaré et les records invalides
avant toute mutation. La restauration d’un snapshot corrompu conserve l’état
précédent de la campagne.

Le test couvre masque hors bornes et état non persistable sur une campagne
active. Build, CTest (`5/5`) sous Xvfb/audio dummy, smoke Vulkan et audits
campagne/code passent.

## Cycle 973 — reprise de campagne active par sauvegarde

Le snapshot campagne version 2 conserve désormais, pour les missions
`Briefing`, `Active`, `Completed` et `Failed`, l’état, le masque d’objectifs et
le loadout. `AC6SESS` passe en version 5 pour transporter ces champs ; les
versions campagne/session antérieures restent lisibles avec l’état historique
`Completed` implicite.

Le test sauvegarde une mission active avec loadout et objectif partiellement
complété, recharge le fichier, puis vérifie que la campagne reste `Active` et
rejouable. Build, CTest (`5/5`) sous Xvfb/audio dummy, smoke Vulkan et audits
campagne/code passent.

## Cycle 972 — input mission et gel de pause

`MissionExecution` accepte maintenant une `InputMappingDatabase` générique.
Les boutons de `InputFrame` sont résolus avant la simulation et dispatchent
les événements HSM. En `Paused`, combat, radio, vagues et séquence ne
progressent plus ; la reprise recommence au tick suivant.

Le test couvre pause/reprise par boutons, axes extrêmes pendant la pause et
bornes de tick. Validation réussie sur un Xvfb explicitement vérifié : build,
CTest (`5/5`), smoke Vulkan et audits campagne/code.

## Cycle 971 — débriefing succès/échec et retour campagne

`FrontendController` expose maintenant `enter_debrief` et
`return_to_campaign`. Le résultat est accepté uniquement depuis `Mission`,
pour la mission sélectionnée, après une exécution terminée ; avec une
campagne, l’état `Completed`/`Failed` doit correspondre au résultat. Le
débriefing conserve les compteurs d’objectifs et l’historique radio jusqu’au
retour campagne.

Le test couvre les deux parcours : succès avec progression campagne et échec
par destruction du joueur, puis retour à `NewGame` avec sélection nettoyée.
Build, CTest (`5/5`) sous Xvfb/audio dummy, smoke Vulkan et audits campagne/code
passent.

## Cycle 970 — raccord frontend vers lancement de mission

`FrontendController::launch_selected` relie désormais l’état `Mission` au
`MissionLaunchDatabase` et à `MissionExecution` sans branche par mission. Le
raccord vérifie l’état frontend, l’identité sélectionnée, la présence du
launch manifest et refuse une seconde activation d’une exécution déjà lancée.

Le test suit le parcours campagne avec loadout valide jusqu’à
`ScenarioState::Gameplay`, puis vérifie qu’un second lancement est refusé.
Build, CTest (`5/5`) sous Xvfb/audio dummy, smoke Vulkan et audits campagne/code
passent.

## Cycle 969 — ordinal des objectifs découplé de leur identité

`MissionExecution` ne déduit plus l’index campagne d’un objectif par
`objective_id - 1`. `MissionScenario::objective_index` calcule l’ordinal
déterministe des objectifs réellement chargés ; les IDs stables peuvent donc
être non contigus sans perdre la progression ni bloquer la réussite.

Le test ajoute une mission avec les objectifs `10` et `20`, vérifie leur
activation/complétion et le passage campagne à `Completed`. Build et CTest
(`5/5`) sous Xvfb/audio dummy passent.

## Cycle 968 — racines retail partiellement qualifiées

L’inventaire de code conserve les sept racines natives et ajoute les adresses
retail qualifiées déjà présentes dans `analysis/address_catalog.tsv` : vtables
campagne/HSM, update du manager, dispatch d’état, factory joueur et update
radio. Ces entrées restent `retail_status=partial` : elles qualifient une
frontière binaire, pas le graphe complet ni les routes sélecteur→DPL.

Validation : `code_inventory=pass roots=7 native_covered=7 retail_partial=6
retail_unknown=1 entries=16` avec le SHA-256 du `default.xex` vérifié. Le
catalogue campagne reste `1 qualified / 1 partial / 13 unqualified`.

## Cycle 967 — inventaire code machine-readable

`reports/ac6-code-reachability-inventory.json` couvre les sept racines
fonctionnelles avec preuves natives et distingue explicitement les racines
retail inconnues. Le validateur fail-closed exige module, callers/callees,
missions, rôle, preuve et gaps pour chaque inconnue.

Résultat initial : `roots=7 native_covered=7 retail_unknown=7 entries=9`; le catalogue
campagne reste `1 qualified / 1 partial / 13 unqualified`. Ce checkpoint ne
transforme aucune hypothèse retail en sémantique.

## Cycle 966 — persistance du playback radio

`MissionExecution::Checkpoint` conserve le snapshot radio actif (message,
assets, temps, durée et état). `AC6SESS` version 4 l’encode avec validation
bornée; les versions v1–v3 restent lisibles avec playback Idle implicite.

Build, CTest (`5/5`) sous Xvfb/audio dummy, smoke SDL3/Vulkan et audit campagne
passent. Les états radio inconnus ou mélangés entre missions sont rejetés.

## Cycle 965 — persistance du séquenceur

`AC6SESS` version 3 ajoute au checkpoint les événements du
`MissionSequenceDirector` avec leur état published/pending. Les identités de
mission, l’ordre, les types, durées et bornes sont validés avant publication;
les formats v1/v2 restent lisibles.

Build, CTest (`5/5`) sous Xvfb/audio dummy, smoke SDL3/Vulkan et audit campagne
passent. Les séquences mélangées entre missions sont rejetées.

## Cycle 964 — séquenceur HSM mission

`MissionSequenceDirector` ordonne les événements objectifs/radio par tick et
rang, et `MissionExecution` les applique avec ses préconditions. Les doublons
sont rejetés et la publication est confirmée seulement après succès.

Build, CTest (`5/5`) sous Xvfb/audio dummy, smoke SDL3/Vulkan et audit campagne
passent. La position publiée du séquenceur reste à sérialiser dans `AC6SESS`;
les événements non publiés doivent être conservés par la frontière runtime.

## Cycle 963 — frontière radio/playback

`RadioPlaybackService` suit la lecture exclusive d’un message, ses assets
audio/sous-titre, sa durée, sa fin et son interruption. `MissionExecution`
publie l’historique seulement après démarrage valide; le playback est gelé
pendant `Paused`.

Build, CTest (`5/5`) sous Xvfb/audio dummy, smoke SDL3/Vulkan et audit campagne
passent. Le décodage XMA et les durées retail restent explicitement derrière
la frontière de service.

## Cycle 962 — directeur de vagues d’unités

`MissionWaveDirector` publie les unités dues par tick de façon atomique dans
`UnitRegistry` et `CombatWorld`; `MissionExecution` l’exécute après chaque
tick. Les doublons sont rejetés et le despawn met les deux registres en accord.

Build, CTest (`5/5`) sous Xvfb/audio dummy, smoke SDL3/Vulkan et audit campagne
passent. Les paramètres retail de vagues restent non qualifiés et ne sont pas
inventés par le runtime.

## Cycle 961 — route frontend campagne

`FrontendController` est raccordé optionnellement à `CampaignProgression` :
disponibilité à la sélection, briefing, loadout au Hangar, début campagne et
état `Active` avant Mission. Le chemin sans campagne reste disponible pour les
fixtures développeur; les missions verrouillées sont rejetées.

Build, CTest (`5/5`) sous Xvfb/audio dummy, smoke SDL3/Vulkan et audit campagne
passent. Les paramètres menu retail et mappings non qualifiés restent
explicitement ouverts.

## Cycle 960 — échec destruction et expiration

`MissionExecution` déclenche automatiquement l’échec sur destruction du
joueur ou dépassement d’un tick limite configuré. `Abort` met à jour le HSM,
la progression campagne active et le débrief; un second abort est rejeté.
Les impacts et dommages externes passent par `CombatWorld::apply_damage`.

Build, CTest (`5/5`) sous Xvfb/audio dummy, smoke SDL3/Vulkan et audit campagne
passent. Le tick limite est une frontière générique et n’est pas présenté
comme une constante retail qualifiée.

## Cycle 959 — persistance du checkpoint de session

`AC6SESS` version 2 persiste désormais le checkpoint MissionExecution complet
(vol, HSM, objectifs, radio et unités de combat), avec bornes, validation
transactionnelle et lecture rétrocompatible de la version 1. Le test couvre
round-trip, corruption sans mutation et fichier v1.

Build, CTest (`5/5`) sous Xvfb/audio dummy, smoke SDL3/Vulkan et audit campagne
passent. Les projectiles en vol restent volontairement exclus et empêchent la
création d’un checkpoint.

## Cycle 958 — checkpoint MissionExecution

`MissionExecution::Checkpoint` regroupe vol, HSM, joueur, objectifs, radio et
unités de combat. La restauration valide toutes les bornes avant publication
et revient à l’état précédent si une partie échoue. Les projectiles en vol
font explicitement refuser un checkpoint pour éviter une reprise non
déterministe.

Le test couvre pause/reprise, objectif actif, unités et corruption HSM. Build,
CTest (`5/5`) sous Xvfb/audio dummy, smoke SDL3/Vulkan et audit campagne
passent. `AC6SESS` ne sérialise pas encore ce checkpoint complet; seule la
combinaison vol/progression est actuellement persistée sur disque.

## Cycle 957 — contrat combat générique

`CombatWorld` modélise les unités actives par faction, santé et rayon de
collision, ainsi que les armes, verrous de cible, projectiles, portée,
cooldown, impacts et dégâts déterministes. `MissionExecution` initialise sa
frontière combat au lancement et expose le verrouillage et le tir sans branche
Mission 1. Le test couvre les doublons, le cooldown, l’impact et la destruction
d’une unité.

Build, CTest (`5/5`) sous Xvfb avec `SDL_AUDIODRIVER=dummy`, et smoke SDL3/
Vulkan passent. Les paramètres retail d’armes/dégâts restent explicitement
non qualifiés et devront venir du catalogue/manifeste avant une revendication
de fidélité.

## Cycle 956 — catalogue campagne 15 missions

`reports/ac6-pal-campaign-catalog.json` contient exactement les missions 1 à
15, les hashes PAL XEX/DATA.TBL, la provenance de chaque ligne, son statut de
qualification, le parse et ses lacunes. L’auditeur fail-closed rapporte
`missions=15 qualified=1 partial=1 unqualified=13`; aucune route inconnue
n’est extrapolée. L’inventaire Mission 01 reste `rows=14`.

La couverture d’artefact est fermée, mais les 13 routes retail non qualifiées
restent un risque ouvert et ne peuvent pas alimenter le runtime.

## Cycle 955 — format de session combiné

`SessionSaveStore` joint `RuntimeSnapshot` et `CampaignSaveSnapshot` dans un
format borné `AC6SESS`, sans modifier `AC6SAVE` ni `AC6CSAV`. Les floats, la
borne du pas fixe, les slots et les enregistrements campagne sont validés;
l’écriture est atomique et la lecture transactionnelle.

Le test couvre round-trip des deux sous-états, restauration campagne, slot nul
et fichier corrompu. Build, CTest (`5/5`) sous Xvfb/audio dummy et smoke
SDL3/Vulkan passent.

## Cycle 954 — débriefing MissionExecution

`MissionScenario::debrief()` et `MissionExecution::debrief()` publient une
vue native du résultat, des compteurs d’objectifs et de l’historique radio.
Les tests couvrent les résultats succès et échec après raccord à la
progression campagne, sans branche par mission.

Build, CTest (`4/4`) sous Xvfb/audio dummy et smoke SDL3/Vulkan passent.

## Cycle 953 — persistance campagne native

`CampaignSaveStore` écrit et lit un format borné `AC6CSAV`, séparé du format
historique `AC6SAVE` du vol. Slots et missions sont triés, les entrées
invalides/tronquées sont rejetées, l’écriture utilise un fichier frère et un
remplacement atomique, et une lecture invalide conserve l’état précédent.

Round-trip, corruption et bornes passent dans le test campagne. Build, CTest
(`4/4`) sous Xvfb/audio dummy et smoke SDL3/Vulkan passent. Le regroupement
dans un format de session unique reste ouvert.

## Cycle 952 — raccord progression / MissionExecution

`MissionExecution` accepte une frontière optionnelle `CampaignProgression`.
Le launch exige l’état `Active`; les objectifs, la réussite et l’échec sont
propagés au HSM campagne après validation des préconditions. Le chemin sans
campagne reste disponible pour les fixtures et adaptateurs développeur.

Le test runtime couvre succès et échec avec objectifs externes, sans branche
par identifiant de mission. Build, CTest (`4/4`) sous Xvfb/audio dummy et
smoke SDL3/Vulkan passent. Le snapshot de campagne n’est pas encore joint au
snapshot de vol dans un fichier de session; la persistance combinée reste
ouverte.

## Cycle 951 — manifeste campagne natif générique

`CampaignProgression` et son chargeur TSV optionnel (`campaign` dans le
manifeste natif) couvrent maintenant les routes explicites sélecteur → DPL →
DATA.TBL, les prérequis, loadout qualifié, objectifs, états de mission et
snapshot déterministe `AC6CAMP`. Le chargement est transactionnel et refuse
doublons de sélecteur, références manquantes, cycles et lignes malformées.
Les snapshots vierges et les ressources physiques partagées restent valides.

Build, CTest (`4/4`) sous Xvfb avec `SDL_AUDIODRIVER=dummy`, et smoke
SDL3/Vulkan passent. Les routes du test sont synthétiques : ce checkpoint ne
qualifie toujours pas les mappings retail des missions 3–15.

## Cycle 934 — boucle d’échec MissionExecution

`MissionExecution` expose maintenant `fail_objective(id)` comme frontière
générique pour destruction/expiration. Le scheduler natif reste immuable dans
les états `Paused`, `Complete` et `Aborted`; un objectif échoué place la
scène en `Aborted` et refuse toute complétion ultérieure. Le test runtime
couvre lancement, tick gameplay, échec, gel de l’état et rejet de `Complete`.

Build CMake, CTest (`3/3`) sous Xvfb avec `SDL_AUDIODRIVER=dummy`, et smoke
SDL3/Vulkan à deux présentations passent. Ce checkpoint ferme uniquement le
contrat HSM natif; il ne constitue pas une preuve retail de la boucle Mission
01 ni une qualification des 15 mappings DPL/DATA.TBL.

## Cycle 891 radios intégrées au manifeste runtime

La table `radios` est validée par `load_runtime`, chargée par
`--present-manifest` et reliée à `MissionExecution::dispatch_radio`. Elle
reste optionnelle. CTest (`2/2`) et smoke Vulkan passent.

## Cycle 890 radios externes

Les messages radio sont maintenant des données externes avec speaker, assets
audio/sous-titres et historique d’émission validé par mission. La lecture
reste séparée dans SDL3. CTest (`2/2`) et smoke Vulkan passent.

## Cycle 889 sanitizer après objectifs

Après l’extension objectifs/manifeste, le build ASan/UBSan reconfiguré passe
CTest (`2/2`) et le smoke Vulkan sans erreur mémoire ni UB. Les fuites des
backends SDL/DBus/DRM restent le seul risque externe connu.

## Cycle 888 validation tables optionnelles

Le loader runtime valide désormais `input` et `objectives` lorsqu’ils sont
déclarés dans le manifeste, avec publication atomique. Le test d’un événement
inconnu confirme le rejet sans mutation. CTest (`2/2`) et smoke Vulkan passent.

## Cycle 887 objectifs externes

Les objectifs sont maintenant chargés depuis une table `objectives` externe,
injectés dans `MissionExecution` et contrôlés par le HSM. Tests de chargement,
gate de complétion et lancement passent ; CTest (`2/2`) et smoke Vulkan aussi.

## Cycle 886 registre d’objectifs générique

Ajout du registre d’objectifs et de la gate HSM : `Complete` exige désormais
tous les objectifs requis, tandis que les missions sans objectifs restent
compatibles. CTest (`2/2`) et smoke Vulkan passent.

## Cycle 885 gate SHA-256 assets qualifiés

`load_runtime` refuse désormais les assets dont le SHA-256 n’est pas un hash
hexadécimal complet de 64 caractères. Les fixtures de test permissives ne
peuvent plus alimenter le chemin produit. CTest (`2/2`) et smoke Vulkan passent.

## Cycle 884 statut du paquet explicite

Le paquet Linux documente maintenant son statut `developer preview` et les
prérequis stricts avant toute revendication retail. Son contenu reste limité
au README, headers et binaire natif ; CTest (`2/2`) et smoke Vulkan passent.

## Cycle 883 paquet Linux minimal

CPack génère `ac6-native-0.1.0-Linux.tar.gz`, contenant uniquement le binaire
et les headers natifs. Le paquet ne contient aucun asset retail et son audit
de strings est propre. CTest (`2/2`) et smoke Vulkan passent.

## Cycle 882 gate CTest headless

Le démarrage headless de `ac6-native` est désormais automatisé dans CTest.
CTest passe à `2/2`; le smoke Vulkan sous Xvfb et l’audit des symboles
interdits restent verts.

## Cycle 881 audit manifestes retail locaux

Les rapports PAL et l’entrée DATA 9 sont présents, mais aucun jeu complet de
tables render et buffers qualifiés n’est disponible localement. La route
`--present-manifest` reste donc correctement non prouvée en positif retail ;
aucune fixture synthétique n’est promue comme preuve.

## Cycle 880 layouts swapchain par image

Le presenter suit désormais l’état de layout de chaque image de swapchain,
évitant une transition `PRESENT_SRC` incorrecte sur les premières acquisitions
des images secondaires. Le smoke couvre deux présentations consécutives.
Build, CTest (`1/1`) et smoke Vulkan double-frame passent.

## Cycle 879 frontière audio SDL3

`SdlAudioDevice` fournit la frontière playback SDL3 séparée, avec queue PCM,
reprise et destruction fail-closed. Le test pousse du silence quand le
périphérique est disponible ; la gate headless passe avec `SDL_AUDIODRIVER=dummy`.
Build, CTest (`1/1`) et smoke Vulkan passent.

## Cycle 878 pause du scheduler mission

`MissionExecution::dispatch` relie les événements HSM au runtime ; en pause,
le scheduler fixe ne progresse plus et les transforms restent stables. La
reprise redémarre au tick suivant. Build, CTest (`1/1`) et smoke Vulkan passent.

## Cycle 877 loaders runtime atomiques

Catalogue, assets et lancements chargent maintenant atomiquement ; les tests
de lignes invalides confirment l'absence de mutation partielle. Build, CTest
(`1/1`) et smoke Vulkan passent.

## Cycle 876 exécution multi-familles

Une mission AirIntercept et une mission Strike sont maintenant lancées dans
le même test via le chemin générique `MissionExecution`; leurs frames et
ownerships sont vérifiés. Build, CTest (`1/1`) et smoke Vulkan passent.

## Cycle 875 clavier de développement SDL

Ajout d’un `SdlKeyboardMapping` séparé du mapping contrôleur, avec validation
des scancodes et conversion key-down/key-up vers `InputFrame`. Tests pitch et
throttle, build, CTest (`1/1`) et smoke Vulkan passent. Le mapping clavier
reste explicitement un outil de développement.

## Cycle 874 gate ASan/UBSan

Build Debug sanitizer, CTest et smoke Vulkan passent sans erreur
AddressSanitizer/UBSan (`detect_leaks=0`). LeakSanitizer signale uniquement
2558 octets dans les backends libSDL3/libdbus/libdrm pendant leur
initialisation ; aucun frame ne pointe vers le code AC6.

## Cycle 873 bornes SDL vers InputFrame

Les axes SDL et la gâchette sont vérifiés aux bornes signées, avec rejet d'un
mapping invalide ; les boutons restent traduits en événements natifs. Build,
CTest (`1/1`) et smoke Vulkan passent.

## Cycle 872 mapping input externe

Le manifeste accepte une table `input`; son chargement est fail-closed et
`--frontend-smoke` s'appuie désormais dessus pour les transitions frontend.
Le masque codé en dur a été retiré. Build, CTest (`1/1`) et smoke Vulkan
passent.

## Cycle 871 différentiel de reprise complet

La smoke sauvegarde/reprise rejoue maintenant la moitié restante de la mission
après restauration et compare la frame finale à la référence. Le test
unitaire compare également deux exécutions restaurées au tick suivant.
Build, CTest (`1/1`) et smoke Vulkan passent.

## Cycle 870 snapshot complet de l’état de vol

Les snapshots sérialisent désormais positions et angles pitch/roll/yaw.
AC6SAVE v2 est écrit atomiquement et les fichiers v1 restent lisibles avec
angles nuls. Tests de restauration/persistance, CTest et smoke Vulkan passent.

## Cycle 869 sauvegarde/replay reliés à la mission

`MissionExecution` publie maintenant snapshot/restauration et
`--services-smoke` vérifie écriture/lecture du save, replay de 30 ticks et
déterminisme des positions. Build, CTest (`1/1`) et smoke Vulkan passent.
Limite connue : le snapshot ne sérialise encore que tick et positions ; les
états angulaires nécessitent un contrat retail avant la reprise complète.

## Cycle 868 frontend natif pilotable

Le produit expose `--frontend-smoke <manifest> <mission_id>` : chargement
externe, sélection mission et cinq événements `StartMission` mènent à l'état
`Mission`, avec refus fail-closed. Build, CTest, smoke Vulkan et audit des
dépendances passent. La preuve frontend retail complète attend toujours les
archives et manifestes qualifiés.

## Cycle 867 validation positive du loader render

Une fixture externe temporaire couvre désormais le chargement atomique du
catalogue, des assets, du lancement, des dix tables render et d'un buffer
géométrique qualifié jusqu'au décodage natif. Elle est explicitement
synthétique et ne remplace pas les manifestes retail. Build, CTest (`1/1`) et
smoke Vulkan passent ; les fichiers temporaires sont nettoyés.

## Cycle 866 swapchain transfer/present

Les images de swapchain déclarent et vérifient maintenant l'usage
`TRANSFER_DST`, requis par `present_frame` pour l'upload de la `WorldFrame`.
La conversion ARGB interne → RGBA8 est couverte par un test déterministe.
Build, CTest (`1/1`) et smoke SDL3/Vulkan sous Xvfb passent. La preuve de
premières frames Mission 01 attend toujours le manifeste retail qualifié et
les tranches de buffers correspondantes.

## Cycle 797 native product boundary

`reconstruction/ace-combat-6` contient maintenant la frontière minimale
`ac6_product_core`, `ac6-native` et un test déterministe. Le runtime reste
fail-closed (`mission_ready=false`) sans manifeste retail qualifié. CMake et
CTest passent (`1/1`), et l’exécutable retourne 2 comme prévu.

## Cycle 798 native asset ID contract

`MissionAssetDatabase` fournit maintenant une résolution par IDs stables avec
chemin de manifeste et SHA-256, en rejetant les entrées nulles, incomplètes ou
dupliquées. Le contrat est testé sans embarquer de données retail.

## Cycle 799 native manifest loader

Le chargement TSV externe (`id`, chemin relatif, SHA-256) est implémenté avec
validation stricte et test temporaire. CTest reste vert (`1/1`).

## Cycle 800 native Mission 01 asset gate

`MissionRuntime` consomme maintenant `MissionAssetDatabase` et exige les IDs
9, 119, 165, 199 et 210 avant de publier `mission_ready=true`. Le test couvre
les états incomplet et complet ; l’exécutable sans manifeste reste fail-closed.

## Cycle 801 native fixed-step replay

L’API `MissionRuntime::tick(fixed_dt, InputFrame)` produit maintenant un
`WorldFrame` déterministe. Un test rejoue 120 ticks identiques sur deux
runtimes et compare les transforms ; CTest reste vert (`1/1`).

## Cycle 802 native scenario ownership

`MissionScenario` fournit un HSM minimal piloté par événements explicites et
un `EntityId` joueur. Les transitions invalides et un sujet non possédé sont
rejetés ; le test couvre les contrôles positif et négatif.

## Cycle 803 native scenario WorldFrame gate

`MissionRuntime` exige maintenant un scénario en état `Gameplay` en plus des
assets qualifiés avant de publier `mission_ready=true`. Le test couvre la gate
fermée puis ouverte par `StartMission`.

## Cycle 804 native unit registry

`UnitRegistry` fournit des unités natives (`EntityId`, owner, asset ID,
activation) et rejette les IDs invalides, l’auto-ownership et les doublons.
Les contrôles positifs et négatifs passent dans CTest.

## Cycle 805 native player binding

`MissionScenario` lie désormais le joueur uniquement à une unité enregistrée
et active via `UnitRegistry`; les entités absentes ou inactives sont refusées.

## Cycle 806 native WorldFrame units

`WorldFrame` publie désormais `active_units` et `player_entity` depuis le
registre et le scénario natifs. Sans ces propriétaires, les valeurs restent
nulles ; CTest couvre le cas lié (`1/1`).

## Cycle 807 native camera/render gate

`WorldFrame` publie une caméra de suivi déterministe dérivée du joueur, et
`VulkanRenderer` accepte uniquement les frames prêtes avec unité/joueur
actifs. Les contrôles de soumission invalide/valide passent dans CTest.

## Cycle 808 native RenderAssets/frontend

Le renderer sépare maintenant `WorldFrame` et `RenderAssets`, exigeant les IDs
9 et 119. Le frontend natif expose le flux Title → New Game → Briefing →
Hangar → Loading → Mission ; CTest couvre les transitions et le refus terminal.

## Cycle 809 native save/replay services

`SaveStore` et `ReplayLog` fournissent sauvegarde/reprise par slot et journal
d’entrées déterministe, séparés des backends Xbox. Les contrôles de slots et
de replay passent dans CTest.

## Cycle 810 native runtime save/restore

`MissionRuntime` exporte et restaure maintenant un `RuntimeSnapshot` via
`SaveStore`, avec rejet des checkpoints invalides. L’audit du binaire natif ne
trouve aucun symbole Xbox/XAM/XMA/Rex/PPC/Xenia ; CTest reste à `1/1`.

## Cycle 811 native replay runner

`MissionRuntime::run_replay` exécute directement les entrées du `ReplayLog`;
deux exécutions identiques produisent les mêmes ticks et transforms. CTest
reste vert (`1/1`).

## Cycle 812 native package audit

L’installation CMake Release ne contient que `bin/ac6-native` et le header
public. Le staging et l’audit `strings` ne révèlent aucune dépendance Xbox,
oracle ou PPC ; CTest reste à `1/1`.

## Cycle 813 native mission catalog

`MissionCatalog` rend les familles de missions et leurs assets déclaratifs,
avec rejet des définitions inconnues ou dupliquées. CTest couvre les contrôles
positifs et négatifs (`1/1`).

## Cycle 814 native external mission definitions

`MissionCatalog` charge maintenant un manifeste TSV externe
`mission_id<TAB>family<TAB>asset_ids`, avec familles qualifiées
`air_intercept`, `strike` et `escort` et IDs d'assets séparés par virgules.
`FrontendController` sélectionne une mission uniquement si elle existe dans ce
catalogue. `MissionRuntime` exige une `MissionDefinition` correspondant a son
`mission_id` et derive la gate `mission_ready` depuis les assets declares,
supprimant le branchement runtime special Mission 01. Build CMake et CTest
restent verts (`1/1`).

## Cycle 815 native frontend launch definition

`FrontendController` expose maintenant la `MissionDefinition` selectionnee
uniquement apres la transition naturelle jusqu'a `FrontendState::Mission`.
`MissionRuntime` peut etre construit depuis cette definition externe, ce qui
relie selection frontend et runtime sans etat force ni branche par
`mission_id`. Les tests n'utilisent plus `assert` pour des effets de bord :
une macro `REQUIRE` active en Release garde la route `advance()` executable
avec `-DNDEBUG`. Validation : build Release, CTest Release `1/1`, CTest ASan
Debug `1/1`.

## Cycle 816 native launch manifest

`MissionLaunchDatabase` charge maintenant un manifeste de lancement externe
`mission_id<TAB>player_entity<TAB>unit_id:owner:asset,...`. La configuration
`configure_mission_launch` instancie `UnitRegistry`, active les unites et lie
le joueur au `MissionScenario` uniquement si le lancement correspond a la
mission courante. Le test runtime passe par catalogue frontend, definition
mission, manifeste de lancement, scenario et registre avant d'ouvrir
`mission_ready`; il ne force plus le joueur via `own_player`. Build CMake et
CTest Release passent (`1/1`). L'audit `strings` ne trouve aucun marqueur
Xbox/XAM/XMA/RexGlue/XenonRecomp/Xenia/PPC dans `ac6-native`; `ldd` reste sur
les bibliotheques Linux standard.

## Cycle 817 native render manifest

`WorldFrame` transporte maintenant le `mission_id` natif, et
`MissionRenderDatabase` charge un manifeste TSV externe
`mission_id<TAB>asset_ids` pour les assets requis par le rendu. `VulkanRenderer`
ne contient plus les IDs 9/119 en dur : il refuse une frame non prete, sans
joueur/unites actifs, sans definition de rendu, avec definition de mission
incompatible ou avec asset de rendu absent. Le test couvre les controles
negatifs et la soumission positive. Build CMake et CTest Release passent
(`1/1`); le scan `strings` reste sans marqueur Xbox/oracle/PPC et `ldd` ne
liste que les dependances Linux standard.

## Cycle 818 native world submission counters

`VulkanRenderer` expose maintenant une surface mesurable hors HUD :
`last_world_asset_count` et `world_asset_submissions`. Les compteurs ne bougent
pas sur les rejets et augmentent uniquement quand une frame prete, une mission,
un joueur, des unites actives, une base d'assets et une definition de rendu
compatible sont tous presents. Cette garde reste volontairement une mesure de
soumission d'assets monde, pas une revendication de pixels monde rendus.
Build CMake et CTest Release passent (`1/1`); le scan `strings` reste sans
marqueur Xbox/oracle/PPC et `ldd` ne liste que les dependances Linux standard.

## Cycle 819 native color/depth readback target

`NativeRenderTarget` ajoute une cible couleur/profondeur native fail-closed
avec `resize`, `clear`, marquage deterministic par asset monde soumis et
`RenderReadback` (`color_coverage`, `depth_coverage`, hashes couleur/depth).
`VulkanRenderer::render` peut maintenant ecrire dans cette cible quand les
gates frame/assets/render definition sont satisfaites. Le test prouve les
dimensions invalides, le clear sans couverture, une soumission positive avec
couverture couleur/profondeur et des hashes reproductibles sur deux cibles.
Cette preuve reste une surface de readback produit, pas encore un rendu mesh
retail Mission 01. Build CMake et CTest Release passent (`1/1`); le scan
`strings` reste sans marqueur Xbox/oracle/PPC et `ldd` ne liste que les
dependances Linux standard.

## Cycle 820 native drawable manifest

`MissionDrawableDatabase` charge maintenant un manifeste TSV externe
`mission_id<TAB>asset_id<TAB>primitive_count`. Le renderer peut exiger une base
de drawables et refuse une soumission si un asset de rendu declare n'a pas de
drawable correspondant. `NativeRenderTarget::draw_world_asset` transforme le
`primitive_count` en couverture couleur/profondeur deterministe, plafonnee pour
rester bornee. Le test couvre manifeste valide, doublon invalide, base
incomplete rejetee, couverture 3+5 primitives et hashes reproductibles. Cette
preuve remplace le pixel unique par asset par une surface drawable, mais ne
revendique toujours pas les meshes retail Mission 01. Build CMake et CTest
Release passent (`1/1`); le scan `strings` reste sans marqueur
Xbox/oracle/PPC et `ldd` ne liste que les dependances Linux standard.

## Cycle 821 native multi-drawable surface classes

`MissionDrawable` porte maintenant `stable_id`, `kind`, `asset` et
`primitive_count`, avec manifeste TSV
`mission_id<TAB>stable_id<TAB>kind<TAB>asset_id<TAB>primitive_count`.
`MissionDrawableDatabase` accepte plusieurs drawables pour un meme asset et les
retrouve par asset ou par ID stable. Le test couvre une surface Mission 01
representative : mapobj sur asset 9, terrain sur asset 119 et sky/cloud sur le
meme asset 119. Le renderer refuse une base incomplete et le readback couvre
15 primitives declarees. Cette structure aligne la surface produit sur les
preuves cycles 760/774, sans encore charger ni dessiner les buffers retail
NDXR/NTXR. Build CMake et CTest Release passent (`1/1`); le scan `strings`
reste sans marqueur Xbox/oracle/PPC et `ldd` ne liste que les dependances Linux
standard.

## Cycle 822 native drawable buffer contracts

Chaque `MissionDrawable` exige maintenant un contrat de buffer qualifie :
`buffer_id`, `vertex_count`, `index_count` et `content_hash`. Le manifeste TSV
devient
`mission_id<TAB>stable_id<TAB>kind<TAB>asset_id<TAB>primitive_count<TAB>buffer_id<TAB>vertex_count<TAB>index_count<TAB>content_hash`.
Les drawables sans contrat buffer sont rejetes par le loader et par
`NativeRenderTarget::draw_world_asset`. Le test rattache les fixtures aux
preuves disponibles : `GIDX268439850` pour `mapobj_m01_l_brg1_n`,
`021/010_NDXR` et hash `7209F3DEB7BD097D` pour un terrain entry 119, et
`entry119/022_FHM/005_FHM` pour sky/cloud. Cette etape qualifie la frontiere de
buffer, mais ne charge pas encore les bytes retail ni ne decode NDXR/NTXR.
Build CMake et CTest Release passent (`1/1`); le scan `strings` reste sans
marqueur Xbox/oracle/PPC et `ldd` ne liste que les dependances Linux standard.

## Cycle 823 native qualified buffer database

`QualifiedBufferDatabase` charge maintenant un manifeste externe
`buffer_id<TAB>path<TAB>byte_size<TAB>fnv64` et verifie les slices par taille
et hash FNV64 avant rendu. `VulkanRenderer::RenderAssets` peut recevoir cette
base et refuse toute soumission si un drawable declare pointe vers un buffer
non verifie. Le test couvre buffers charges non verifies, verification
positive de trois slices factices, hash negatif, rejet renderer avant
verification et soumission positive apres verification. Aucun byte retail n'est
embarque; les chemins restent externes et bornes. Build CMake et CTest Release
passent (`1/1`); le scan `strings` reste sans marqueur Xbox/oracle/PPC et
`ldd` ne liste que les dependances Linux standard.

## Cycle 824 native geometry metadata parser

`NativeGeometryDatabase` charge maintenant une metadata geometrique stricte
depuis un buffer externe deja verifie. Le format borne retenu pour cette etape
est `AC6GEO1<TAB>vertex_count<TAB>index_count<TAB>primitive_count`; les valeurs
doivent correspondre au `MissionDrawable` (`vertex_count`, `index_count`,
`primitive_count`) avant que le renderer accepte la base geometrique.
`NativeRenderTarget::draw_world_geometry` refuse toute divergence entre
metadata et contrat drawable. Le test couvre trois buffers verifies, une base
geometrique incomplete rejetee, puis une soumission positive avec readback
deterministe. Cette etape transforme des slices verifiees en metadata native,
mais ne decode pas encore le format retail NDXR/NTXR. Build CMake et CTest
Release passent (`1/1`); le scan `strings` reste sans marqueur
Xbox/oracle/PPC et `ldd` ne liste que les dependances Linux standard.

## Cycle 825 native bounded NDXR header parser

`NativeGeometryDatabase` remplace le format temporaire `AC6GEO1` par un header
borne `NDXR<TAB>1<TAB>vertex_count<TAB>index_count<TAB>primitive_count`.
Le parser exige le magic `NDXR`, la version `1` et des compteurs correspondant
au `MissionDrawable`. Le test verifie que l'ancien `AC6GEO1` est rejete, que
les trois slices factices NDXR chargees produisent une metadata
`source_format=NDXR`, et que la base geometrique incomplete bloque toujours le
renderer. Cette etape remplace la metadata ad hoc par une frontiere NDXR
bornee, sans encore parser les blocs/polygones/streams retail complets ni NTXR.
Build CMake et CTest Release passent (`1/1`); le scan `strings` reste sans
marqueur Xbox/oracle/PPC et `ldd` ne liste que les dependances Linux standard.

## Cycle 826 native bounded NDXR sections

Le parser NDXR borne exige maintenant, apres le header, les sections
`VTX<TAB>vertex_count<TAB>stride`, `IDX<TAB>index_count<TAB>index_size` et
`POLY<TAB>primitive_count<TAB>flags`. Les counts doivent correspondre au header
et au `MissionDrawable`; les sections inconnues, dupliquees, manquantes ou
incoherentes sont rejetees. `NativeGeometryMetadata` expose les compteurs de
sections et `draw_world_geometry` les reverifie avant readback. Le test couvre
mapobj, terrain et sky/cloud avec sections valides, rejet de l'ancien
`AC6GEO1`, et rejet d'une section `VTX` incoherente. Cette etape ferme une
frontiere sectionnelle NDXR, mais ne lit pas encore les bytes de streams
vertices/index ni les descriptors polygones retail complets. Build CMake et
CTest Release passent (`1/1`); le scan `strings` reste sans marqueur
Xbox/oracle/PPC et `ldd` ne liste que les dependances Linux standard.

## Cycle 827 native bounded NDXR stream sizing

`NativeGeometryMetadata` expose maintenant `vertex_stride`, `index_size`,
`vertex_byte_size` et `index_byte_size`. Le parser NDXR borne exige un marqueur
`DATA` apres les sections, puis verifie que le payload verifie par
`QualifiedBufferDatabase` couvre au minimum les bytes declares par
`vertex_count*stride + index_count*index_size`. Les index sizes acceptes sont
2 ou 4. Le test couvre les tailles derivees du terrain, le rejet d'une section
`VTX` incoherente et le rejet d'une slice trop courte. Une regression locale a
ete corrigee : le payload n'est plus interprete comme une section NDXR. Build
CMake, CTest Release et CTest Debug passent (`1/1`); le scan `strings` reste
sans marqueur Xbox/oracle/PPC et `ldd` ne liste que les dependances Linux
standard.

## Cycle 828 native bounded NDXR stream samples

`NativeGeometryDatabase` produit maintenant un `DecodedGeometry` borne depuis
le payload `DATA` deja verifie : au plus quatre positions natives `x/y/z`
lues en float little-endian au debut de chaque vertex stride, et au plus huit
indices 16/32-bit little-endian. Le parser refuse les strides inferieurs a 12
bytes, les lectures incompletes et tout index echantillonne hors
`vertex_count`. Le test remplace les payloads factices par des streams binaires
coherents, verifie les samples terrain `021/010_NDXR`, et couvre le cas
negatif d'un index hors bornes. Cette etape reste un decodeur d'echantillons
borne, pas un rendu mesh retail complet. Build CMake et CTest Release passent
(`1/1`); le scan binaire reste sans marqueur Xbox/oracle/PPC, `ldd` ne liste
que les dependances Linux standard, et aucun branchement produit Mission 01 ou
asset 9/119 hardcode n'est present dans `include`/`src`.

## Cycle 829 native geometry-driven target coverage

La voie render cible avec `NativeGeometryDatabase` ne retombe plus sur le
marquage synthetique par `primitive_count`. `VulkanRenderer` exige maintenant
metadata et `DecodedGeometry` pour chaque drawable quand une base geometry est
fournie, puis `NativeRenderTarget::draw_world_geometry` marque couleur/depth a
partir des samples vertex/index decodes. Les guards verifient la frame prete,
le drawable, la metadata, les samples non vides, les positions finies et les
indices bornes. Le test compare explicitement le fallback synthetique
(`15` pixels pour les drawables fixtures) a la voie geometry-driven, dont la
couverture et les hashes doivent diverger. Le harness `REQUIRE` affiche
maintenant fichier/ligne/expression pour les futures regressions. Build CMake
et CTest Release passent (`1/1`); le scan binaire reste sans marqueur
Xbox/oracle/PPC, `ldd` ne liste que les dependances Linux standard, et aucun
branchement produit Mission 01 ou asset 9/119 hardcode n'est present dans
`include`/`src`.

## Cycle 830 native decoded geometry bounds

`DecodedGeometry` porte maintenant une bounding box native derivee des samples
vertex verifies. `NativeGeometryDatabase::load_verified` calcule
`min/max x/y/z`, refuse les positions non finies et exige une bounds valide
avant d'accepter le buffer. `NativeRenderTarget::draw_world_geometry` consomme
cette bounds en plus des samples vertex/index pour marquer la cible
geometry-driven, avec guards sur les valeurs finies et l'ordre min/max. Le
test verifie les bounds attendues du terrain fixture `021/010_NDXR` et couvre
le rejet d'un payload verifie contenant un float NaN. Build CMake et CTest
Release passent (`1/1`); le scan binaire reste sans marqueur Xbox/oracle/PPC,
`ldd` ne liste que les dependances Linux standard, et aucun branchement produit
Mission 01 ou asset 9/119 hardcode n'est present dans `include`/`src`.

## Cycle 831 native drawable transform manifest

La voie geometry-driven consomme maintenant une transform native explicite par
drawable. `MissionTransformDatabase` charge un manifeste externe
`mission_id<TAB>stable_id<TAB>tx<TAB>ty<TAB>tz<TAB>sx<TAB>sy<TAB>sz`, refuse
les valeurs non finies, les scales non positifs et les doublons. Quand
`RenderAssets::geometries` est fourni, `VulkanRenderer` exige aussi une
transform pour chaque drawable; `NativeRenderTarget::draw_world_geometry`
projette les samples et la bounds apres transformation locale -> world-space.
Le test couvre un manifeste complet, un scale nul invalide, le rejet
geometry-sans-transform, la reproductibilite des hashes avec transforms
identiques et la divergence des hashes quand les transforms changent. Build
CMake et CTest Release passent (`1/1`); le scan binaire reste sans marqueur
Xbox/oracle/PPC, `ldd` ne liste que les dependances Linux standard, et aucun
branchement produit Mission 01 ou asset 9/119 hardcode n'est present dans
`include`/`src`.

## Cycle 832 native camera projection target

`NativeRenderTarget::draw_world_geometry` remplace la projection modulo par une
projection caméra native derivee de `WorldFrame.camera_*`. La projection
construit une base view fail-closed, applique un near plane, un FOV vertical
borne et rejette les points hors frustum. Les samples vertex/index et bounds
transformes locale -> world-space ne marquent plus la cible que lorsqu'ils
sont projetables par cette caméra; le rendu geometry echoue si aucun sample
n'est visible ou si la caméra est degeneree. Les fixtures transforms sont
placees dans le cone de la caméra de suivi, le test garde la divergence de
hashes avec le fallback synthetique et ajoute un rejet de frame a caméra
target=origin. Build CMake et CTest Release passent (`1/1`); le scan binaire
reste sans marqueur Xbox/oracle/PPC, `ldd` ne liste que les dependances Linux
standard, et aucun branchement produit Mission 01 ou asset 9/119 hardcode
n'est present dans `include`/`src`.

## Cycle 833 native material pipeline manifest

La voie geometry-driven consomme maintenant un materiau/pipeline explicite par
drawable. `MissionMaterialDatabase` charge un manifeste externe
`mission_id<TAB>stable_id<TAB>shader_permutation<TAB>depth_test<TAB>depth_write<TAB>blend_mode<TAB>base_color`.
Les modes acceptes sont `opaque`, `alpha` et `additive`; les couleurs a alpha
nul, permutations vides, booleens invalides, modes inconnus et doublons sont
rejetes. Quand `RenderAssets::geometries` est fourni, `VulkanRenderer` exige
maintenant geometry, transform et material pour chaque drawable.
`NativeRenderTarget::draw_world_geometry` utilise le material pour depth
test/write, alpha/additive blend et shading base-color, au lieu de coder la
couleur depuis l'asset. Le test couvre manifeste valide, mode invalide, rejet
geometry-sans-material, reproductibilite des hashes, divergence avec transforms
modifiees et divergence colorimétrique avec materials modifies. Build CMake et
CTest Release passent (`1/1`); le scan binaire reste sans marqueur
Xbox/oracle/PPC, `ldd` ne liste que les dependances Linux standard, et aucun
branchement produit Mission 01 ou asset 9/119 hardcode n'est present dans
`include`/`src`.

## Cycle 834 native texture sampler manifest

La voie geometry-driven consomme maintenant une texture/sampler explicite par
drawable. `MissionTextureDatabase` charge un manifeste externe
`mission_id<TAB>stable_id<TAB>texture_id<TAB>sampler_filter<TAB>sampler_address<TAB>content_hash`.
Les filtres acceptes sont `nearest` et `linear`; les address modes acceptes
sont `wrap` et `clamp`; texture IDs vides, hashes nuls, modes inconnus et
doublons sont rejetes. Quand `RenderAssets::geometries` est fourni,
`VulkanRenderer` exige maintenant geometry, transform, material et texture pour
chaque drawable. `NativeRenderTarget::draw_world_geometry` incorpore le hash
texture et les modes sampler dans le shading, en gardant les textures externes
et sans embarquer d'asset retail. Le test couvre manifeste valide, sampler
invalide, rejet geometry-sans-texture, reproductibilite des hashes, divergence
avec transforms/materials modifies et divergence colorimétrique avec textures
modifiees. Build CMake et CTest Release passent (`1/1`); le scan binaire reste
sans marqueur Xbox/oracle/PPC, `ldd` ne liste que les dependances Linux
standard, et aucun branchement produit Mission 01 ou asset 9/119 hardcode
n'est present dans `include`/`src`.

## Cycle 835 native shader permutation manifest

La voie geometry-driven consomme maintenant un catalogue de permutations shader
explicite. `ShaderPermutationDatabase` charge un manifeste externe
`shader_permutation<TAB>vertex_layout<TAB>texture_fetches<TAB>constant_count<TAB>render_target_format`.
Les layouts vides, fetch/constant counts nuls, formats RT inconnus et doublons
sont rejetes. Quand `RenderAssets::geometries` est fourni, `VulkanRenderer`
exige maintenant que chaque `MissionMaterial::shader_permutation` resolve dans
ce catalogue en plus de geometry, transform, material et texture.
`NativeRenderTarget::draw_world_geometry` incorpore l'ID shader, le layout, les
fetches texture, les constantes et le format RT dans le shading. Le test couvre
manifeste valide, format invalide, rejet geometry avec textures mais sans
shaders, reproductibilite des hashes et divergence colorimétrique quand les
layouts/fetches/constants changent. Build CMake et CTest Release passent
(`1/1`); le scan binaire reste sans marqueur Xbox/oracle/PPC, `ldd` ne liste
que les dependances Linux standard, et aucun branchement produit Mission 01 ou
asset 9/119 hardcode n'est present dans `include`/`src`.

## Cycle 836 native render target manifest

La voie geometry-driven consomme maintenant une definition de render target par
mission. `MissionRenderTargetDatabase` charge un manifeste externe
`mission_id<TAB>width<TAB>height<TAB>color_format<TAB>depth_format<TAB>depth_enabled`.
Les dimensions nulles ou excessives, formats couleur/depth inconnus,
incoherences `depth_enabled`/`depth_format` et doublons mission sont rejetes.
Quand `RenderAssets::geometries` est fourni, `VulkanRenderer` exige maintenant
une definition RT en plus de geometry, transform, material, texture et shader.
`NativeRenderTarget::draw_world_geometry` verifie que la cible fournie
correspond aux dimensions declarees, que le format couleur correspond au
format RT du shader, et que les flags depth du material sont compatibles avec
la surface depth. Le test couvre manifeste valide, depth incoherent invalide,
rejet geometry sans RT definition et rejet d'une cible aux mauvaises
dimensions. Build CMake et CTest Release passent (`1/1`); le scan binaire
reste sans marqueur Xbox/oracle/PPC, `ldd` ne liste que les dependances Linux
standard, et aucun branchement produit Mission 01 ou asset 9/119 hardcode
n'est present dans `include`/`src`.

## Cycle 837 native render pass manifest

La voie geometry-driven consomme maintenant une passe render explicite par
mission. `MissionRenderPassDatabase` charge un manifeste externe
`mission_id<TAB>pass_id<TAB>order<TAB>color_target<TAB>depth_target<TAB>clear_color<TAB>clear_depth`.
Pour ce cycle, la passe `world` est exigée et doit cibler `main_color` et
`main_depth` quand la RT mission active depth. Les ordres nuls, targets
inconnues, depths clear hors `[0,1]`, doublons et incohérences avec la RT sont
rejetés. Quand `RenderAssets::geometries` est fourni, `VulkanRenderer` exige
maintenant une definition RT et une passe `world`. `draw_world_geometry`
verifie la cohérence passe/RT et incorpore pass id, ordre, targets et
clear_color dans le shading. Le test couvre manifeste valide, clear depth
invalide, rejet geometry avec RT mais sans passe, et divergence des hashes
quand l'ordre de passe change. Build CMake et CTest Release passent (`1/1`);
le scan binaire reste sans marqueur Xbox/oracle/PPC, `ldd` ne liste que les
dependances Linux standard, et aucun branchement produit Mission 01 ou asset
9/119 hardcode n'est present dans `include`/`src`.

## Cycle 838 native render resolve manifest

La voie geometry-driven consomme maintenant un resolve explicite de la passe
`world` vers la cible finale. `MissionRenderResolveDatabase` charge un
manifeste externe
`mission_id<TAB>source_pass<TAB>source_target<TAB>destination_target<TAB>mode`.
Pour ce cycle, `world/main_color -> present` est exigé; les destinations
inconnues, sources incohérentes, modes inconnus et doublons sont rejetés. Quand
`RenderAssets::geometries` est fourni, `VulkanRenderer` exige maintenant
definition RT, passe `world` et resolve `world`. `draw_world_geometry` verifie
la cohérence resolve/passe et incorpore source target, destination et mode dans
le shading. Le test couvre manifeste valide, destination invalide, rejet avec
RT+passe mais sans resolve, et divergence des hashes quand le mode resolve
change. Build CMake et CTest Release passent (`1/1`); le scan binaire reste
sans marqueur Xbox/oracle/PPC, `ldd` ne liste que les dependances Linux
standard, et aucun branchement produit Mission 01 ou asset 9/119 hardcode
n'est present dans `include`/`src`.

## Cycle 839 native intermediate render target route

La voie geometry-driven supporte maintenant une surface intermediaire declaree
pour la passe monde. `MissionRenderTargetDefinition` porte un `target_id`, et
`MissionRenderTargetDatabase` accepte plusieurs targets par mission. Le
manifeste RT devient
`mission_id<TAB>target_id<TAB>width<TAB>height<TAB>color_format<TAB>depth_format<TAB>depth_enabled`.
La passe `world` peut cibler `world_color`, puis le resolve verifie
`world_color -> present`. `VulkanRenderer` resolve la RT source depuis
`pass.color_target`, exige aussi la RT destination du resolve, et
`draw_world_geometry` verifie que la RT source correspond a la passe active.
Le test couvre `world_color` + `present`, rejet d'une destination `present`
absente, et conserve les divergences de hash pour pass/resolve. Build CMake et
CTest Release passent (`1/1`); le scan binaire reste sans marqueur
Xbox/oracle/PPC, `ldd` ne liste que les dependances Linux standard, et aucun
branchement produit Mission 01 ou asset 9/119 hardcode n'est present dans
`include`/`src`.

## Cycle 840 native MSAA resolve contract

La voie geometry-driven declare maintenant le `sample_count` des render
targets. Le manifeste RT devient
`mission_id<TAB>target_id<TAB>width<TAB>height<TAB>sample_count<TAB>color_format<TAB>depth_format<TAB>depth_enabled`.
La fixture Mission 01 utilise `world_color` en 4x MSAA et `present` en 1x.
Le resolve nominal est `msaa_resolve`; un `copy` direct 4x -> 1x est rejete
par `draw_world_geometry`. La destination est maintenant passee au draw et
verifiee par mission, target, dimensions, format, depth inactive et contrat de
samples. Les samples source/destination influencent le hash de shading pour
que les changements de contrat soient observables.

Validation: build CMake et CTest passent (`1/1`), le scan binaire reste sans
marqueur Xbox/oracle/PPC, `ldd` ne liste que les dependances Linux standard, et
aucun branchement produit Mission 01 ou asset 9/119 hardcode n'est present dans
`include`/`src`. Aucune screencap retail n'est produite par `ac6-native` a ce
stade: le chemin courant expose un readback framebuffer deterministe dans les
tests, pas une fenetre SDL/Vulkan native visible.

## Cycle 789 canonical consumer device join

An oracle-only Midasm probe after `0x8234D150` now proves four consecutive
indexed LY reads with `r3=0x8290DE3C`, the exact canonical device qualified in
cycle 782. The ownership join `device -> 0x8234D110 -> lhzx` is therefore
closed for the null-input window. The current oracle profile still stalls in
the black diagnostic frame before Mission 01, so no non-zero LY or child-flight
response is claimed. Evidence:
`reports/cycle-789-canonical-consumer-device-join.md`.

## Cycle 785 indexed axis-load qualification

The canonical Ghidra project now has one bounded indexed consumer candidate:
`0x8234D150 lhzx` in `0x8234D110`, reached from table `0x8201250C` through
`lwz -4(r11)`, `addi +0x14`, and `rlwinm` scaling. This is the retained
consumer of the sign-split axis fields; `r3` still needs runtime identity
qualification against live device `0x8290DE3C` before native flight semantics
are implemented. Evidence: `reports/cycle-785-indexed-axis-load-qualification.md`.

## Cycle 784 canonical LY direct-reader rejection

A read-only scan of canonical Ghidra project `ace-combat-6` finds only one
direct halfword read at displacement `+0x3E`: `0x8217EAB0` in `0x8217E9F0`.
Its qualified caller passes a serialization block at `param_2+0x6D8`, not the
live XInput device, so it is rejected. The next consumer search must cover
indexed `lhzx` data flow from table `0x8201250C`; a direct-displacement
watchpoint would miss the proven sign-split path. Evidence:
`reports/cycle-784-canonical-input-direct-reader-rejection.md`.

## Cycle 782 canonical pitch-input boundary

The bridge diagnostic reaches the black Mission 01 HUD with a live
`CModeTaskGame`, progressing mission-manager update and active
`UpInput/UpObj/UpCam/UpRadio`. The live UnitManager has 230 objects and one
RTTI-qualified `CAce6UnitPlayer`. Raw controls reach XAM. Canonical factory
analysis proves that this is a 256-byte wrapper, so the previous `+10672`
flight-model join is rejected rather than diagnosed as failed initialization.
Cycle 779 closes the next runtime join: the list at `+216/+220` owns one stable
child `0xB2470100`, update slot `+0x3C` executes, and the transform copied to
the player changes across flight frames. Cycles 780–781 add a one-variable
pitch test with null windows: `ly=32767` is followed immediately by a transform
response far above null drift, reproductible sur deux runs. Cycle 781 rejette
directement `child+380/+382/+536/+538` comme commande pitch : ils restent nuls
sur le front d'entrée malgré un contrôle positif tardif du probe. The physical
response is supported. Cycle 782 closes the direct canonical ingestion at
`0x8234D378`: XAM `ly=32767`, raw `device+0x4E=0x7FFF` and canonical
`device+0x3E=0x7FFF` share one timestamp and return together to zero. The
canonical-to-child consumer remains open, so G8 is not yet qualified. Ghidra
Bridge also rejects the historical `0x821CE088/0x82215418/0x82215210` input
roles for the canonical project; the lower historical sections in this file
must not be used as current PAL address proof. This is not stock gameplay
proof. `SDL_AUDIODRIVER=dummy` is required for qualified Xvfb runs. Evidence:
`reports/cycle-782-canonical-pitch-input.{md,json}`.

## Cycle 739 runtime PAC integrity gate

The authoritative PAL runtime now proves exact owner joins and byte equality
for mission entries 9, 119, 165, 199 and 210. Each decoder record is resolved
through `DATA00.PAC + source_offset + compressed_size + decompressed_size`;
runtime decoded bytes and normalized recursive FHM manifests match the offline
reference 5/5. A bounded-diagnostic defect that rewrote entry 199 on every
cache-flush-loop hit was corrected to one successful dump per entry. The same
fresh-profile route then reached Mission 01 and gameplay normally.

This closes decoder, guest-copy and allocation corruption as the current
rendering cause. White-aircraft material binding and the gameplay render graph
remain open. Evidence: `reports/cycle-739-runtime-pac-integrity-gate.{md,json}`.

## Cycle 734 renderer diagnostic checkpoint

The authoritative Linux Vulkan runtime deterministically reproduces a textured
Mission 01 hangar, white aircraft over textured cinematic terrain, and a black
gameplay world with green HUD. Identity and evidence are in
`reports/cycle-734-qualified-vulkan-baseline.{md,json}`. The asset gate remains
open because the non-generated decoder adapter reads incorrect register roles
at the frozen cache-flush callsite and emits no decoded payload. Renderer fixes
are deferred until exact archive/offset/size joins and runtime hashes pass.

## Product boundary

The deliverable is a native Windows/Linux reconstruction under
`reconstruction/ace-combat-6/`. This workspace retains the Xbox 360 PowerPC
evidence, Ghidra/re-agent state and proprietary files. The native product does
not embed an emulator or retail assets. Its renderer backend is Vulkan, owned
by the AC6 reconstruction. It has no runtime dependency on Xbox libraries,
RexGlue, Xenia or their compatibility services.

XenonRecomp, RexGlue and Xenia are permitted only as temporary evidence,
instrumentation and behavioral-oracle tooling. Fixes made there must expose a
reproducible contract that can be reimplemented in the portable product; they
are not themselves completion evidence for the final architecture.

## Implemented native foundation

- bounds-checked big-endian `DATA.TBL` parsing;
- typed selector, PAC-bank and storage-class accessors;
- validation against the actual `DATA00.PAC` and `DATA01.PAC` sizes;
- monotonic range, expanded-size and stored-class invariants;
- recovered `ACE6::CAce6Uncompress` modes: custom LZ, raw DEFLATE and stored;
- exact pi/Machin-derived XOR key generation with the retail 256-entry cycle;
- bounded single-entry extractor and exhaustive archive verifier;
- recursive FHM asset-manifest generator with a depth-16 guard, neutral
  metadata fields and exact signature-only resource classes;
- bounded `NDXR` model parser with exact object/polygon descriptors, named
  index/vertex/additional/name clumps, retail positions and 16-bit topology;
- bounded `NSXR` wrapper parser with five safe neutral region slices;
- bounded `MATE` wrapper parser with three safe neutral region slices;
- portable packed-handle resolver for the XEX `0x40`-stride runtime record
  pool, with native bounds checking;
- exact campaign selector-to-DPL-resource mapping, DPL key formatting and
  registry CRC generation for the `0x820a85e0` load path;
- bounded child-1 `MDLP` directory parsing matching `0x8228e988`,
  `0x8228e9a8`, and `0x8228e9b8`;
- bounded fixed-record `Scene/` path-table parsing and absolute FHM ancestry
  mapping for entry-9 children 22 and 23;
- exact index-based resolution of all 553 entry-9 Scene paths through adjacent
  resource FHMs, with bounded `GYZ` wrapper parsing and `NFIC CUT` group proof;
- fixed-stride `GYZ` record-table parsing and nine-chunk `NFIC CUT` parsing,
  including its bounded event stream and identifier/string dictionary;
- native replay of the dictionary-proven cut/frame/camera-event state and a
  deterministic raw-plus-structured Scene MOP/CUT remaster export;
- bounded Tcam position/orientation/FOV sampling, sparse generic MOP transform
  sampling, and an SDL3 Linux Scene shell rendering two joined retail aircraft;
- portable C++20 implementation, synthetic tests and sanitizer gate.
- address-bounded native input normalization, configurable logical-button
  remapping and pressed/released edges from `0x821CE088`, `0x82215140` and
  `0x82214F88`, with a visible non-flight SDL diagnostic.
- frame-lifecycle-gated `NFIC CUT` camera replay: `MoveCamera` is rendered
  only while a dictionary-proven `FrameStart` remains active and its payload
  frame matches the active frame; a matching four-byte `FrameTerminate` and
  then `CutTerminate` close that state before another camera event can be
  accepted.
- SDL3 native Scene-player operation for Bluetooth Xbox controllers: SDL
  standard A/D-pad/Start events feed the recovered raw masks and then the
  existing `0x821CE088` normalization and logical-button table. Back toggles
  the native camera inspection presentation, Guide exits, and F11 toggles
  fullscreen. This is an evidence-backed input route into the bounded CUT
  player, not a claim of retail flight or pause-menu behaviour.
- The entry-9 native Scene inspector exposes serialized Scene-group selection
  interactively (`--scene-group INDEX`), keeping the selected index and FHM
  archive path in its title and JSON. This is a deterministic inspector route,
  not evidence that campaign progression activates the selected group. The
  asset-backed gate covers six independently replayable joined CUT groups:
  `22.1.0` (120 camera states), `22.1.1` (80), `22.1.5` (220), `22.1.10`
  (430), `23.1.2` (210), and `23.1.4` (420). The child-23 groups prove that the
  native chain also handles the independently serialized child-23 subtree; this
  does not claim a retail mission-order relationship between any of them.
- The inspector now has a fail-closed replay catalog generated from the same
  entry-9 loader: only the six groups above have both a bounded camera replay
  and joined world. `[`/`]` and the Bluetooth Xbox left/right shoulder buttons
  move through that catalog, reset the native playback state, and retain the
  exact serialized index/FHM path in the title and JSON. This is native UI for
  comparison and inspection only, not a retail campaign transition.
- exact `CModeTaskTitleMovie` update-state representation from `0x821B9048`:
  auxiliary-ready/skip/complete predicates, three-tick mode handoff and the
  generic `{1,3}` flow tuple are exposed as side-effect-free native effects.
  It is intentionally isolated from campaign launch and mission gameplay.
- The pinned Windows Xenia Canary oracle now has a Wine/Vulkan launcher with
  keyboard mode and an AZERTY-compatible `Start=Return` preflight. A user
  session confirms the startup order developer splash -> copyright ->
  skippable cinematic -> title. This is a startup-horizon observation only;
  it does not qualify title-to-campaign selection, mission loading or flight.
  See `reports/cycle-97-xenia-wine-startup-sequence.md`.

## Executed retail gate

The native inspector validated the supplied European revision:

- 926 records and two PAC banks;
- 466 records in bank 0 and 460 in bank 1;
- 800 compressed-class records and 126 stored-class records;
- 2,914,429,232 stored bytes represented by table entries;
- 5,424,368,676 expanded bytes declared;
- every stored range is monotonic and within its selected PAC.
- all 926 payloads decode to their exact declared expanded size;
- all 926 decoded payloads begin with the `FHM ` inner-container signature.
- all 926 top-level FHM directories pass bounded parsing, with 4,820 member
  slots and 2,811 non-empty members.

This proves the table layout, range semantics, encryption key stream and payload
codec against the complete supplied retail archive. It does not yet parse the
resource-specific inner payloads or make the game playable. The recursive FHM
gate emits 56,514 rows and finds 5,435 nested containers, 8,006 `NTXR`, 2,228
`NDXR`, 1,549 `NFIC`, 1,293 `Scene`, 1,029 `NFH`, 733 `MATE`, 546 RIFF and
11,204 empty slots. The remaining 23,861 non-empty payloads retain the neutral
`binary` class and their four-byte prefix only.

The first resource-specific slice now validates all 8,006 `NTXR` records:
7,993 complete wrappers have bounded descriptor, `eXt`, `GIDX` and texture-data
boundaries, while 13 are explicit 16-byte header references. Xenos descriptor
bit fields remain neutral until their dimensions, format and tiling semantics
are independently proven.

The second resource-specific slice validates all 2,228 `NDXR` records. Every
wrapper has version byte 2, an exact declared size and three nonzero,
16-byte-aligned region lengths whose sum remains inside the payload. Their
graphics semantics remain deliberately unnamed; see `NDXR_STRUCTURE_REPORT.md`.

The XEX-backed `0x822c2148` slice now decodes its exact `0x30`-stride NDXR
record loads and flag-bit gate within a fail-closed native capacity. The retail
gate covers 250,766 bounded slots; 248,325 pass the original bit test. The four
float meanings remain unnamed because no caller has yet been recovered; see
`NDXR_FUNCTION_822C2148_REPORT.md`.

The 22-instruction leaf `0x822c31e8` now has a bounded portable search for its
signed key at record offset `0x24`. Eleven XEX call references establish the
runtime search contract. Corpus correlation finds only keys 2 and 4 among ten
profiled fixed caller keys, so direct identity between serialized slots and the
complete runtime container remains unproved; see
`NDXR_FUNCTION_822C31E8_REPORT.md`.

The four-instruction leaf `0x822c3150` now has a portable resolver for its exact
26-bit packed index and `0x40`-byte runtime-record stride. The native span model
replaces the XEX base pointer loaded at container offset `+0x20` and rejects
incomplete or out-of-capacity records. Its three-instruction tail wrapper at
`0x822c8e48` supplies a handle from `+0x04` and a container from `+0x2c`.
Record semantics remain neutral; see `FUNCTION_822C3150_RECORD_POOL_REPORT.md`.

All 51 `NSXR` wrappers now pass exact-size and five-region boundary validation.
The first four offsets are constant at `0x60/0x70/0x80/0x90` in this corpus;
the fifth has 26 observed values. Region meanings remain unnamed; see
`NSXR_STRUCTURE_REPORT.md`.

All 733 `MATE` wrappers now pass three-region boundary validation. The first
offset is `0x30` throughout this corpus, but the next two have 18 and 66 values;
their material/shader meanings remain unassigned. See `MATE_STRUCTURE_REPORT.md`.

## Next native front

The DPL-registry-to-physical-archive assignment is now closed for the campaign
low-id path. The XEX path proves that bounded campaign selector 1 maps through `0x821b6e58` to
resource id 9, key `DPL::[0x9,0]` and hash `0xfc76b3c6`, followed by recursive
registry lookup, then through `0x821d1128` to physical `DATA.TBL` record 9.
Archive decoding is no longer the blocker: all 926 entries decode,
and 1,293 `Scene` payloads are structurally inventoried.

The DPL and physical index namespaces are now proven non-identical in general:
valid DPL ids exceed `0x75e`, while the direct archive branch is restricted to
ids below `0x39d`; larger ids take a special-resource route. The native
`ac6-mission-diagnostic` reports selector 1's physical entry as 9. See
`DPL_ARCHIVE_HANDLE_CHAIN.md`.

The decoded entry-9 diagnostic now deterministically reproduces 1,111 recursive
manifest rows, including 44 `Scene` payloads. Its child 1 is a 29,097,984-byte
`MDLP` directory with 94 elements. Exact XEX helpers expose those elements to
`0x820a7070`, which makes two complete registration passes and later selects
individual elements while constructing runtime objects. See
`ENTRY9_CHILD1_CONSUMER_REPORT.md`.

The construction-loop owner is now typed by Microsoft RTTI as
`X360UnitManager`, derived from `ACE6::CAce6UnitManager`. Its base constructor
clears an exact 256-slot pointer/word table. The entry-9 virtual factory returns
a runtime object whose `+0x50/+0x54/+0x58` floats are cleared before a selected
MDLP resource is attached at `+0x15c`. This establishes a gameplay unit-owner
and transform-bearing object boundary, but not yet an aircraft subtype, spawn
position, or semantic function name. See `ENTRY9_X360_UNIT_MANAGER_REPORT.md`.

Factory selector 1 is now proved by final-vtable RTTI as
`ACE6::CAce6UnitPlayer`; selector 2 is `ACE6::CAce6UnitOtherPlayer`, both with
`ACE6::CAce6Unit` in the hierarchy. The native address-based factory-evidence
API assigns RTTI only to these two cases; selectors 3 through 6 retain exact
vtable/callback addresses with unknown RTTI. It also exposes the exact zero
vector at `+0x50` while keeping aircraft, spawn and position flags false. GCC
and Clang ASan+UBSan each pass 16/16 tests.

Selectors 3 through 6 now have exact vtable/callback evidence but no RTTI;
their callback ignores the supplied record argument. Owner slot `+0x14` is
bounded for direct selectors 2/3 and its complete 15-key selector-4 table.
Those callbacks bind record/nested pointers without a proved transform write.
The native API mirrors these rows while keeping RTTI, aircraft, spawn and
position false; see `ENTRY9_X360_UNIT_MANAGER_REPORT.md`.

The reusable factory-object method dossier now keeps slot `+0x50` (the
post-factory record callback) separate from slot `+0x54` (the selected-pointer
predicate). Selectors 1/2 share `0x822ddbe8` for both slots; selectors 3–6 use
`0x82297a40` and `0x822974c8` respectively. This is an exact method-family
reuse boundary, not a complete C++ class or a gameplay semantic claim. It is
implemented by `function_820a7f48_virtual_method_evidence` and covered by the
unit-factory tests.

The post-factory chain is bounded through retail insertion `0x8226f050` and
frame traversal `0x8226ecb0`, called from `0x8226a310` at `0x8226a508`.
Native evidence records the exact 1024-entry collection layout, queried
virtual slots, flag masks and direct-call arguments. No executable edge to
`MissionAircraft`, spawn semantics, or a proved position field was found, so
all corresponding claim fields remain false.

Separately, retail `0x822f31e8` proves that the selected global object's
`+0x50/+0x54/+0x58` floats are formatted as `X/Y/Z`. The native evidence marks
that local XYZ semantic true, while keeping the identity of that selected
object as an entry-9 factory result, MissionAircraft, and spawn position false.
The intervening slot-`+0x54` condition is now exact: `0x8226f050` tests only
the low byte returned in `r3`. Selectors 1/2 preserve their dynamically
allocated, merely `0x10`-aligned object pointer; selectors 3-6 do the same when
flag `+0x60 & 0x100` is clear. The specifically proved entry-9 record-callback
armed path for selectors 3-6 returns zero. Runtime arena address residue still
blocks a static entry-9-to-XYZ identity, and debug XYZ formatting remains no
evidence of a spawn operation.
An executable-wide direct-store inventory adds only collection initializer
`0x8226eb88`, which zeros `+0x1008/+0x100c` at `0x8226eb98/0x8226eb9c` while
also clearing the exact 1024-entry and 16-pointer layout. All other literal
offset stores belong to different structures. Therefore `0x8226f050` is the
sole nonzero writer of these selected pointers; no alternate retail path
closes a particular entry-9 result to the debug XYZ object.

All 94 elements are now individually bounded and inventoried. They are FHM
packages containing 292 `NDXR`, 381 `MATE`, 86 `NTXR`, 47 nested FHM and 942
neutral binary members. No `Scene` signature occurs inside child 1. The
per-element retail artifact is
`reports/entry9-child1-mdlp-inventory.csv` with recorded source and artifact
SHA-256 provenance.

The 44 entry-9 `Scene` payloads are now exactly localized outside child 1:
32 occur below top-level child 22 and 12 below child 23. They are flat,
non-empty tables of NUL-terminated `Scene/` paths in fixed `0x80`-byte records,
with 553 records total. The absolute-offset artifact is
`reports/entry9-scene-inventory.csv`; see
`ENTRY9_SCENE_PATH_TABLE_REPORT.md`.

All 553 paths now resolve by record index to the same-index member of an
immediately preceding resource FHM. Every one of the 553 payloads satisfies a
bounded `GYZ` wrapper contract, and each of the 44 path/resource pairs has an
immediately preceding `NFIC CUT` sibling. The complete deterministic mapping is
`reports/entry9-scene-path-resolution.csv`; see
`ENTRY9_SCENE_RESOURCE_RESOLUTION_REPORT.md`.

The resolved `GYZ` payloads now expose 3,105 bounded `0x30`-stride records.
The first `Tcam__c01.mop` has three records, while its adjacent `NFIC CUT`
payload proves the serialized sequence `CutStart`, `FrameStart(1)`, then
`MoveCamera` through chunk `0x3040` and its `0x3041` dictionary. All 44 state
payloads validate, covering 169,908 event records. See
`ENTRY9_TCAM_NFIC_CUT_REPORT.md`.

The exact XEX NFIC event path is now recovered through `0x8236eda0`,
`0x8236e2e0`, `0x8236da78`, `0x8236dc70`, `0x8236db48`, and dispatcher
`0x8236b920`. `MoveCamera` now resolves one-based Scene object 1 to path and
resource index 0 (`Tcam__c01.mop`) and its second high half to the Tcam key.
The first CUT yields 120 native camera states. The runtime target is known to
sit at dispatch-context first-pointer offset `+0x10`, but its dynamically
supplied vtable address remains open; see `NFIC_XEX_EVENT_CONSUMER_REPORT.md`.
Native `select_initial_scene_camera` now fails closed unless the first three
events are dictionary-backed `CutStart`, `FrameStart(1)`, and zero-flag
`MoveCamera(object 1, frame 1)`, followed by the bounded adjacent-FHM index
join. This proves the initial camera inside a selected Scene group. Runtime
activation of physical group `22.1.0` is still open, so archive-first traversal
is not claimed as retail mission-scene selection. Static environment geometry
also remains open and no synthetic substitute is generated; see
`MISSION_VISUAL_BOOTSTRAP_REPORT.md`.

An SDL3 Linux shell now loads decoded entry 9 or reaches that same payload from
the proved campaign-selector-1 -> DPL resource 9 -> physical `DATA.TBL` entry
9 route. It locates the first Scene group, replays those camera states, and
derives each first-frame serialized `Rigid`/`AnimRigid` Scene command from the
NFIC dictionary (`0x2003`/`0x2004`) and joins the resulting one-based ids 3
through 18 to their same-index MOP transform and asset-key-matched MDLP model.
The supplied archive yields 16 animated native objects, 39,393 vertices and
57,271 indices at the final CUT frame. Objects 3/4 remain the two exact
MATE/NDXR/NTXR diffuse-bound `r_f16c`/`r_f18f` joins (52 and 63 polygons); the
other 13 joined objects remain geometry-only until their material identities
are closed. Timeline, inspection orbit/zoom and Tcam-toggle controls are
present. The smoke reports `world_renderer=native-partial`; this remains CUT
presentation, not a player-aircraft, mission-spawn, collision, weapon, or
flight-control claim. See `AC6_LINUX_SCENE_SHELL_REPORT.md`.

The native shell no longer implicitly binds every inspection request to the
first traversed Scene triplet. `collect_scene_groups` now enumerates each
structurally valid adjacent `NFIC CUT`/resource-FHM/`Scene` group in stable FHM
directory order and retains its dotted archive provenance. The campaign route
can explicitly inspect a selected index with `--scene-group INDEX --smoke`.
Group 0 remains `22.1.0` (120 camera samples, 16 joined CUT-local objects),
while the independently executed group-1 gate is `22.1.1` (80 camera samples,
17 joined CUT-local objects). Both remain selected serialized presentation
groups; neither is promoted to a campaign activation, spawn or flight claim.

The first CUT's native world join is now frame-lifecycle verified rather than
being a first-frame-only assumption. `collect_nfic_frame_scene_object_tracks`
replays `CutStart`/`FrameStart`/`FrameTerminate`/`CutTerminate`, accepts only
dictionary-backed zero-flag `Rigid`/`AnimRigid` commands whose frame agrees
with the active lifecycle state, and rejects malformed or unterminated input.
The Linux shell requires every one of its 120 replayed camera frames to carry
the same ordered 16-object membership as frame 1 before presenting a
persistent world. This is a CUT-local serialization fact, not evidence of a
mission object lifetime beyond that CUT.

The native player now exposes a narrow, fail-closed campaign-to-CUT session
boundary. `CampaignSceneSessionState` accepts only the independently verified
selector-1 -> `DPL::[0x9,0]` -> physical `DATA.TBL` record-9 route. Its native
launch action can start a ready CUT once, synchronizes only running/paused or
completed playback, and rejects other selectors, records, restart attempts and
unexpected playback state. The SDL shell reports that native session phase in
campaign mode. This is an operable Linux presentation transition, not a claim
that the unresolved retail campaign menu or mission activation path has been
recreated.

The closest executable-side dynamic track consumer is now bounded at
`0x8236bf20`: it validates a frame interval and mode, walks the object track
array and forwards both floating inputs to `0x8236eab0`, which selects a key and
dispatches the property update. Deterministic frame-1/frame-120 capture proves
the joined aircraft and camera state change under native scrubbing/playback.
This is cinematic control, not yet player flight input; see
`FUNCTION_8236BF20_DYNAMIC_TRACK_CONSUMER_REPORT.md`.

A distinct player-input path is now bounded. `0x821CE088` polls four devices,
normalizes XInput-layout button masks into four `0xA0`-stride canonical states,
and `0x82215140` maps those states through 32 configurable masks.
`0x82214F88` computes exact just-pressed and just-released fields. The SDL shell
routes Return through the recovered raw-A mapping and the retail default table
to logical slots 0 and 23, then to a visible diagnostic marker. No flight or weapon meaning is
assigned yet; see `FUNCTION_821CE088_PLAYER_INPUT_REPORT.md`.

`0x821BE268` now closes the complete 32-mask default table for all four
controller blocks in the third logical-input context. Raw A reaches retail
logical slots 0 and 23; eight analog sources occupy slots 10–17. A corrected
15,333-function export found opaque pressed-state consumers and one generic
slot-10 threshold reader, but no write to proven aircraft state. The shell
therefore retains its separate diagnostic and does not move or fire the
aircraft; see `FUNCTION_821BE268_DEFAULT_BINDINGS_REPORT.md`.

The first consumer classification is now closed without gameplay relabeling.
RTTI proves `0x8214C038` is a virtual method of `CSelectAircraftManager`, while
the separate `CSelectAircraftCamera` vtable is identified independently. The
analog-assisted routine entered at `0x820DB500` can measurably OR a positive
directed analog sample into current, just-pressed, and repeat conditions, but
its damaged action-to-axis switch table and event-only receiver do not prove a
player-aircraft or camera mutation. The bounded condition slice is native and
tested; re-agent passes the smallest complete leaf `0x821B3870` in one round.
See `FUNCTION_820DB500_CONSUMER_EFFECTS_REPORT.md`.

The exact three-state subtransition observed at manager `+0x8D54` is now
native as `function_8214c038_next_selection_state`: `0 -> 1`, `1 -> 2`, and
`2 -> 1`; unobserved values fail closed. GCC and Clang ASan/UBSan test it as
part of their 17-test matrices. This remains aircraft-selection presentation
state only and is deliberately not connected to in-flight controls, weapons,
or camera motion.

The event receiver is now identified by pointer identity rather than matching
offsets. The SWG context initialization at `0x820DBD90..0x820DBF84` allocates
the sole 0x150-byte object constructed by `0x8237EDB0`, stores it at context
offset `+0x04`, and later passes that exact pointer to `0x8237E4C0`. The
complete owner flow at `0x820DBF30` also initializes an optional callback/data
pointer at receiver `+0xE8`. The first receiver writes are pointer coordinates
(`+0x124/+0x128`) and a key bitset/code (`+0x12C..+0x14C`), not player or
camera state. Its next synchronous handoff uses an interface object whose
first word is `0x8205A8EC`; the `+0x28/+0x2C` entries are `0x820D99F8` and
`0x820D9A28`, both interior entries in the complete graphics shadow-state
flow. The table owner is now proven, but its entry ABI and gameplay ownership
remain unresolved, so no additional native helper was added. See
`FUNCTION_8237E4C0_EVENT_RECEIVER_REPORT.md` and
`reports/cycle-99-event-receiver-owner-initialization.md`.

The formerly opaque `0x820D9A28` target is now decomposed inside its complete
`0x820D99C0..0x820D9B38` host flow. Its `+0xAA0..+0xAEC` writes are five
`float4` values in a graphics shadow-state bank: the downstream
`0x821E24D8` consumes dirty masks, hardware-register ranges `0x2000..0x2280`,
and command-stream cursors. This is render submission state, not an owned
player/camera transform. The mismatch between the receiver callback ABI and
the interior table entry remains a runtime-table question; see
`FUNCTION_820D99C0_GPU_STATE_REPORT.md`.

The alternative logical-input consumer at `0x821B9048` is also closed by
owner identity. Its vtable locator resolves to RTTI type
`CModeTaskTitleMovie`; `0x821B9110` is only an interior bit test within that
update. Logical bits 0 or 4 can skip/advance the auxiliary title-movie object,
after which a three-tick state emits the generic global-flow tuple `{1, 3}`.
No active-aircraft, mission actor, spawn, or gameplay-camera object is obtained
or written, so this route is a typed non-gameplay blocker and no native helper
was added. See `FUNCTION_821B9048_TITLE_MOVIE_FLOW_REPORT.md`.

The 553 MOP/GYZ and 44 CUT resources now have a deterministic remaster-ready
export under `remaster-export/`, preserving raw bytes, structured tables,
provenance, relations, offsets and checksums. Three generated trees compared
byte-identical; see `SCENE_REMASTER_EXPORT_REPORT.md`.

Next, pivot from an independently typed gameplay-mode, active-aircraft, spawn,
or gameplay-camera owner and trace its mutations backward to logical input by
pointer identity. Do not continue through the title-movie consumer or promote
the GPU shadow-state table to gameplay state. A runtime object/table trace may
still determine whether the `+0xDC` interface target after `0x8237E4C0` is
replaced before dispatch. In parallel,
extend the world join across the remaining first-CUT objects, decode
MATE/NTXR presentation state, and prove the retail camera rotation/projection
order. The dynamically supplied vtable/class behind dispatch-context
first-pointer offset `+0x10` remains a bounded parity question rather than a
blocker for the now-observable native geometry.
The static initialization and vtable provenance of the type-`0x98` service
behind `0x820a7070` are now closed: `0x826a0728 -> 0x826a0708 ->
0x820674d8`, with exact `+0x18/+0x1c/+0x10/+0x24` targets. The result is
allocated/cursor-backed at `0x120`/`0x10`, stored at owner `+0x15c`, and
re-injected through service slot `+0x10`; see
`reports/cycle-136-type98-service-vtable.md` and
`reports/cycle-137-type98-result-lifecycle.md`. The remaining question is the
runtime/business identity of that result and its connection to a proven draw
submission or scene traversal anchor. Then recover the runtime pool construction, Xenos
descriptor bit fields, the inner meanings of neutral
`NDXR`/`NSXR` regions, and a `MATE` consumer. Retain the existing D3D resolution
anchors for the future resolution-independent renderer.

The immediate post-processing is also bounded: `0x822383d0/0x82238408` read a
bounded index-3 view, `0x821d65c0` performs hierarchical key lookup,
`0x822a1258/0x822a9690` copy fixed `0x60`/`0x40`-byte sub-states, and
`0x82286210` marks an auxiliary state after `0x82284e88`. See
`reports/cycle-138-type98-postprocessing-contracts.md`. The payload identity
and any draw/flight consumer remain unknown; no human session is required for
the next static step.

The caller frontier for `0x822c2148` is now closed at the instruction level.
`FindDirectCallsTo.java` finds exactly three direct sites in the same raw worker
`0x82105bb8..0x82106354`: `0x82105ccc`, `0x82105fb8` and `0x821061c0`. All use
the same ABI (`r3` three-float output, `r4` scalar output, `r5` result of the
indirect slot `context+0x00+0x5c`, `r6` low 16 bits of the table word), test the
low return byte, then quantize and clamp state values to `0..0xf`. Ghidra's
export still has no caller and does not assign the raw worker to a containing
function; this is an export/boundary correction, not a semantic identification.
See `reports/cycle-139-ndxr-caller-boundary.md`. The resource pointer, table
consumer and any draw/flight relation remain unknown; the next static step is
to resolve the indirect slot and its writers. No human session is required.

The worker entry is now reconciled as well. `FindPpcBranchesTo.java` finds
exactly two callers of the catalogued entry `0x82105ba8`, at `0x820fbbd4` and
`0x820fcf3c`; both pass the same owner register (`r31`) in `r3`. The contiguous
body starts at `0x82105bb8`, after the common PPC preamble, so the apparent
missing function boundary is an analysis artifact. Both callers dispatch a
slot at `+0x13c` through their first object word before entering the worker,
while the worker dispatches `+0x5c` through `context+0x00`. Their post-return
writes remain offset-qualified and do not prove scene, renderer or flight
semantics. See `reports/cycle-140-ndxr-worker-entry-owner.md`; the next static
target is the dynamic vtable/field provenance and slot writers. No human
session is required.

The table at `0x8205c980` is now an observed vtable candidate: an initializer
writes it at owner offset zero around `0x820f9dfc`, and its words `+0x5c` and
`+0x13c` resolve to `0x82101be0` and `0x821002f0`. The same table also contains
the two worker callers at `+0x10c` (`0x820fbc28`) and `+0x110`
(`0x820fa9c0`), a strong method-family cross-match. The two leaf contracts are
confirmed locally, but their use by the dynamic `r31` owners is not yet proved.
In particular, the worker's `rlwinm r4,r31,0x10,0x17,0x1f` produces a 9-bit
field while `0x82101be0` dereferences `r4+0x1c`; the encoding/provenance of
that value remains open. Do not yet call it a record pointer or assert that
this candidate slot is the `r5` producer at the three worker calls. See
`reports/cycle-141-ndxr-vtable-slots.md` and the qualification in
`reports/cycle-142-ndxr-vtable-provenance-qualification.md`; the next static
target is the writer/provenance chain for the context field `+0x28`, `r31` and
`r4`.

The subobject provenance is now tighter: the allocation path at `0x8212a2a8`
calls `0x820f9dc8` with `outer+0x14`, and that initializer writes
`0x8205c980` at the subobject's offset zero. The same table contains the two
worker-caller methods at `+0x10c` (`0x820fbc28`) and `+0x110` (`0x820fa9c0`),
which is a strong method-family cross-match. The outer object has a distinct
observed vtable candidate at `0x8205d6c0`; do not merge the two layouts. The
worker consumes the subobject-family field `+0x28`, but the exact table value
and the effective `+0x5c` implementation remain runtime-qualified because the
worker still forms a 9-bit `r4` field before the leaf's apparent pointer
deref. See `reports/cycle-143-ndxr-subobject-vtable-provenance.md`.

The `+0x28` field is now structurally qualified as a resource pointer table:
`0x82234e08` performs a bounded pointer-table lookup, and `0x820fa9c0` stores
its literal-index-`0xb` result into subobject `+0x28` (with a later conditional
zeroing path). The worker then uses this field as the base of its indexed word
walk. This closes the table-production shape, not the entry semantics or the
effective `+0x5c` override; the `r4` 9-bit/address discrepancy remains open.
See `reports/cycle-144-ndxr-resource-table-field28.md`.

Cycle 145 corrects the vtable distinction. The outer object at `0x8212a2a8`
uses the distinct candidate table `0x8205d6c0`; its word at `+0x5c` is
`0x820731bc`, not the worker leaf. The call to `0x820f9dc8` with `outer+0x14`
writes `0x8205c980` at the subobject offset zero, and that subobject table is
the only statically coherent one for the worker family (`+0x5c -> 0x82101be0`,
`+0x13c -> 0x821002f0`, callers at `+0x10c/+0x110`). This closes the
outer/subobject separation, not the dynamic instance provenance. The 9-bit
`r4` versus `lhz 0x1c(r4)` contradiction remains explicitly open; no pointer or
`r5` semantic is promoted. See
`reports/cycle-145-ndxr-subobject-dispatch-correction.md`.

Cycle 146 adds a second static constructor path. `0x82183960` prepares a
parent-owned subobject at `param_1+0x611` (`r31+0x1844`) and calls
`0x820f9dc8`, which again writes `0x8205c980` at subobject offset zero before
calling `0x820f9e78` and `0x822b65e8` on adjacent members. In the raw body of
`0x820fa9c0`, the local descriptor at `r1+0x50` is queried at several literal
indices; the bounded pointer-table lookup `0x82234e08(index=0xb)` is stored at
subobject `+0x28`, while neighboring indices populate `+0x0c..+0x2c`. This
strengthens the structural `resource_pointer_table` qualification without
assigning a NDXR or renderer meaning to its entries. The worker still forms
`r4=(r31>>16)&0x1ff` before its indirect dispatch, so the `r4+0x1c` leaf
contradiction remains `needs-dynamic-evidence`; no human session is required.
See `reports/cycle-146-ndxr-second-constructor-resource-lookups.md`.

Cycle 147 corrige une ambiguïté de portée entre le champ mémoire `owner+0x5c`
et le slot vtable `vtable+0x5c`. Le dump headless de `0x82101a18` montre un
résolveur de ressources qui normalise un chemin, initialise une adresse de
sortie et renvoie un handle/pointeur contrôlé; `0x82101b28` reste le
constructeur de chemin distinct. `0x820fbc28` remet plusieurs champs à zéro et
alimente `+0x0c..+0x2c` par des résolutions séparées. Pour `+0x28`, le retour
est produit avec `owner+0x5c` comme adresse de sortie, puis peut être libéré et
annulé lorsque le type vaut `4`. Cette preuve renforce la forme de résolution
de ressource sans donner de sémantique NDXR/draw/vol. La contradiction du
worker (`r4` 9 bits contre feuille lisant `r4+0x1c`) reste
`needs-dynamic-evidence`; voir `reports/cycle-147-ndxr-resource-resolver-field-separation.md`.

Cycle 148 exclut une fusion erronée avec le chemin gameplay entry-9. Les
écritures `object+0x28/+0x5c` du `X360UnitManager` utilisent sa vtable
`0x82055190` et son constructeur `0x82273880`; elles ne sont pas des writers du
sous-objet NDXR, dont le constructeur `0x820f9dc8` écrit la vtable `0x8205c980`
sur `outer+0x14` (ou sur le second parent qualifié). Les références headless
vers `0x8205c980` ne montrent que l'écriture du constructeur
(`0x820f9dfc`). Les offsets numériques communs ne permettent donc aucune
identité d'objet. La frontière worker/`r4` reste `needs-dynamic-evidence` et
aucune action humaine n'est requise; voir
`reports/cycle-148-ndxr-entry9-nonmerge.md`.

Portfolio scheduling is Pharaoh first, AC5 second and AC6 third.

## First material/texture identity boundary

The aircraft model/texture MDLP pairs `76/77` and `78/79` now connect 115 NDXR
polygon texture-id references to exact paired NTXR GIDX values. There are 227
exact occurrences of those identifiers in the four associated MATE payloads.
The first matched NTXR is now a proven six-entry wrapper. Its `0x10002215`
entry decodes natively as a 512x512 single-level BC3 atlas with Xenos tiled-2D
addressing and `8-in-16` endian conversion; exact logical consumption is
262,144 bytes. MATE batch ordinals now map exactly to NDXR polygon descriptors,
material first-texture identifiers match NDXR and NTXR, and every non-restart
NDXR index is local (`index_oob=0`). The two aircraft present 52 and 63 proved
textured polygons respectively; only two first-aircraft UV-incomplete batches
remain wireframe. The frame remains conservatively `native-partial`. See
`AC6_MATERIAL_TEXTURE_LINK_REPORT.md`.

## Canonical motion/resource separation (cycle 211, 2026-07-18)

The canonical PAL image confirms a distinct `CX360MotionRequestManager` vtable
at `0x8205cd90` (`CX360MotionRequestManager`), with slot `+0x08` dispatching
records tagged `0x11` or `0x8181`. `0x82136100` and `0x821371d8` forward
object fields `+0x1a4/+0x1a8` to this manager. A raw producer block at
`0x82127eb0..0x82127f30` fills those fields and calls `0x82136168`, but its
function boundary and object type remain unknown.

Separately, `0x82226c20`, `0x8228e9e8`, `0x8228fc80`, `0x82293d08`,
`0x82374590` and `0x82374978` consume `+0x15c` as resource/property/table
state. This offset homonym must not be merged with the unit manager or motion
manager without pointer provenance. No static edge to the shared owner
receiver, selector-1 campaign identity, `CutTerminate` consumer, or a flight
owner was recovered. AC6 remains `native-partial`, boundary `scene_complete`;
no human action is required for this tranche. See
`reports/cycle-211-canonical-motion-resource-boundary.md`.

## Canonical motion-record producer boundary (cycle 212, 2026-07-18)

The direct callers of `0x82118a50` were rechecked on the canonical PAL image.
The helper normalizes tags `0x11` and `0x8181`; raw resource-parser sites
`0x82128c90`, `0x8212a100`, `0x8212a23c` and `0x8212a598` resolve `entry+0xd0`,
store it in local fields `+0x14/+0x1c/+0x3c/+0x24`, normalize the record and
set `0x826948c0 = 1`. The raw block `0x82127f30 -> 0x82136168` remains an
unknown object initialization path writing `+0x18c..+0x1a8`; its function and
type are not recovered.

This strengthens the resource-record classification but still does not join
the motion manager to the shared owner receiver, selector-1 campaign identity,
`CutTerminate` consumer or a flight/camera owner. AC6 remains
`native-partial`, boundary `scene_complete`; no human action is required.
See `reports/cycle-212-motion-record-producer-boundary.md`.

## Mission 1 Vulkan boundary (cycles 674–681, 2026-08-03)

The no-force Mission 1 route now has a compact pass catalog. The hangar F-16C
and the overhead cinematic terrain are textured, while the aircraft meshes in
that cinematic remain white. Cycle 675 records 243 draws, 243 viewport records
and 616 texture records with no null texture-view bind; the aircraft submits
the distinct `D5B4F4A878949938` pixel-shader family and samples guest bases
`0x06B30000`/`0x045FB000`. This is a cutscene material/view, fetch or shader
semantic boundary, not missing geometry or a global PAC-load failure. The
black gameplay world remains a separate render-target/world-pass boundary;
cycle 674's direct-host-resolve A/B did not change it. Cycle 676's enriched
catalog stalled before campaign entry and is tooling-only. See
`reports/cycle-675-676-vulkan-pass-frontier.md`.

Cycle 679's bounded material-view replay closes the signed/unsigned-view
hypothesis for the white cutscene aircraft: `D5B4F4A878949938` consistently
selects a non-null unsigned BC3 view (`guest_format=20`, Vulkan format 137),
including when the translated shader requests signed sampling. The known-good
hangar family selects the analogous unsigned BC1 view. The next evidence
boundary is therefore cutscene material content/UV/constant or mip-selection
behaviour: cycle 675 already shows the same tiled/endian/swizzle BC3 path
working for hangar/sky surfaces. The AC6-owned Vulkan renderer must carry this
as a material contract, not as a RexGlue-specific workaround.

Cycle 680 adds a hashed ucode manifest for 244 shaders. The D5B4 pixel shader
contains one explicit `tfetch2D` from interpolated `r0.xy`/`tf0`; both observed
cutscene vertex shaders fetch UV data at attribute offset 6 and export it as
`o0.xy`. The cutscene mesh is therefore not textureless at the shader-contract
level. The remaining renderer evidence boundary is the runtime sample/mip
content or the D5B4 lighting/constant path after sampling. See the cycle-680
section of `reports/cycle-675-676-vulkan-pass-frontier.md`.

Cycle 681 exercised the sample-content branch with a zero replacement at the
D5B4/tf0 sample boundary. The marker was observed, the route reached the
cutscene and flight HUD, and no null bind or guest fatal appeared. The aircraft
remained white and the world remained black, so sample content/mip alone is
deprioritized. The next material evidence must capture D5B4 constants or final
pixel output; the native side now has a generic fail-closed Vulkan material
binding contract in `include/ac6/vulkan_material.h` rather than a shader-hash
workaround. See `reports/cycle-681-d5b4-sample-ab.md`.

Cycle 682 compiles the bounded `ac6_log_d5b4_constants` hook, but its fresh
and launch-route attempts stop at the campaign/window transition before a
D5B4 draw (`[ac6-d5b4-const]` count 0). They are harness-only and are not a
reason to spend another unchanged full replay. The runner now retries SDL
window focus for a bounded 10 seconds; the next oracle must begin from an
existing-save/scene-window gate. See
`reports/cycle-682-d5b4-constants-harness.md`.

Cycle 683 tested that gate against the only local copied profile available
from cycle 675. The save browser shows all three slots empty (`MISSION ----`,
`DIFFICULTY LEVEL ----`, no flight time); the source `save.dat` is therefore a
new-game container, not a completed Mission 1 checkpoint. The bounded route
reached `selector44=3` and `type28=6`, then stopped before `type28=8` and
emitted zero D5B4 constant records. Its captures and hashes are retained in
`reports/cycle-683-d5b4-existing-save-window.md`; this is save/harness
evidence, not a renderer result. The state-driven runner was corrected to
match only lines after an append-only follow-log baseline, preventing stale
dialog states from satisfying later waits.

## Native progression contract (cycle 684, 2026-08-03)

`reconstruction/ace-combat-6/include/ac6/campaign_progression.h` now defines
the generic native path from a campaign selector through the mode-1 DPL
resolver and direct `DATA.TBL` index to loadout, briefing, active objectives,
completion, prerequisite unlock and a deterministic save snapshot. The builder
fails closed on empty/duplicate mission definitions, invalid objective counts,
missing prerequisites and prerequisite cycles. Loadout acceptance requires
explicit aircraft/weapon identities and capability-data validity; no force-ready
or launch override is involved.

The deterministic test drives synthetic selector 1→DPL 9→entry 9 and selector
2→DPL 10→entry 10 through the same state machine, proving Mission 2-style
unlock without claiming selector 2's retail payload. CTest is now 46/46; the
exact hashes and boundary are in
`reports/cycle-684-native-campaign-progression-contract.md`. The next native
step is to supply a qualified mission manifest/resource loader and connect this
state to the Vulkan-owned scene/gameplay shell.

## Bounded campaign resource loader (cycle 685, 2026-08-03)

`reconstruction/ace-combat-6/include/ac6/campaign_resource_loader.h` now
attaches a bounded read/decode contract to every resolved
`CampaignResourceRoute`. It selects the DATA.TBL bank, checks the exact
`offset + stored_size` range, copies only that PAC slice, and dispatches the
existing encrypted stored/raw-DEFLATE decoder with the qualified catalog index.
Missing banks, invalid catalog indices, unknown storage classes, truncated
ranges and decode failures are named fail-closed results. No whole-PAC buffer,
filesystem assumption or guest pointer is part of the interface.

The synthetic test proves the stored/decrypted path and all five failure modes.
The full native suite is now 47/47; exact artifacts and the boundary are in
`reports/cycle-685-native-campaign-resource-loader.md`. This remains synthetic
resource evidence: selector 2 and any retail payload are still unqualified.

## Qualified mission manifest gate (cycle 686, 2026-08-03)

`CampaignMissionManifestEntry` now records the expected DPL id/variant, physical
DATA.TBL entry, PAC bank, offset and stored/expanded extents. The manifest
builder resolves the selector and compares every field before delegating to the
same generic progression state machine. Wrong physical indices and wrong
expanded sizes are rejected before loadout/objective state exists.

The two-mission synthetic test still proves Mission 1 completion and Mission 2
unlock/completion through one pipeline; selector 2 remains synthetic. The full
suite is 47/47. See
`reports/cycle-686-native-qualified-mission-manifest.md` for exact hashes and
the remaining retail/Vulkan boundary.

## Scene-shell resource integration (cycle 687, 2026-08-03)

`tools/scene_shell.cpp` now consumes the same generic resource loader instead
of duplicating selector, DATA.TBL and decode logic. It supplies declared PAC
sizes and bounded file-range callbacks; I/O errors and short reads become
explicit loader failures. The SDL shell therefore does not map or copy a full
PAC and shares the native route/decode seam.

The loader, progression and scene smoke tests pass 4/4; the complete suite is
47/47. Exact artifacts are in
`reports/cycle-687-native-shell-resource-integration.md`. This is still native
plumbing evidence only: selector 2 and Mission 2 retail payload/runtime remain
unqualified.

## Transactional native campaign runtime (cycle 688, 2026-08-03)

`CampaignRuntimeState` now joins the qualified manifest, bounded PAC loader and
generic progression state without SDL, Vulkan or guest-pointer dependencies.
Selection is transactional: a missing, truncated or undecodable resource
restores the prior progression state. Loadout, start, objective and completion
events require the selected resource; completion releases it after unlocking
dependents.

The two-mission synthetic runtime test decodes distinct payloads, completes
Mission 1, unlocks/completes Mission 2 through one session, and verifies the
rollback invariant on an empty PAC span. The full suite is 48/48. See
`reports/cycle-688-native-campaign-runtime-session.md`; this is not a retail
Mission 2 claim.

## Native Vulkan campaign submission (cycle 689, 2026-08-03)

`campaign_vulkan_frontend.h/.cpp` now defines the AC6-owned backend boundary:
an active `CampaignRuntimeState`, decoded resource, qualified
`VulkanMaterialBinding` and matching DPL/DATA.TBL route produce one
`CampaignVulkanFrame`. Missing material and route identity drift fail closed.
The record carries mission status, resource identity, decoded byte count and
material binding without re-resolving IDs or touching guest memory.

The deterministic frontend test passes alongside the runtime tests; the full
suite is 49/49. See
`reports/cycle-689-native-vulkan-campaign-submission.md`. This is a submission
contract only: no Vulkan command backend or retail Mission 2 claim exists yet.

## Native Vulkan resource lifetime seam (cycle 691, 2026-08-03)

`CampaignVulkanResourceLifetime` now isolates actual Vulkan object ownership
behind acquire/release callbacks. It enforces one in-flight resource, monotonic
generation leases, stale-completion rejection and retryable release failure;
the portable layer never stores a `Vk*` handle.

The deterministic lifetime test and the full suite pass 2/2 and 50/50. Exact
hashes and the backend boundary are in
`reports/cycle-691-native-vulkan-resource-lifetime.md`. Queue synchronization,
presentation and real Vulkan object creation remain unimplemented by design.

## Headless AC6-owned Vulkan backend (cycle 692, 2026-08-03)

`vulkan_backend.h/.cpp` now provides an optional real Vulkan implementation:
instance, physical device, queue family, device, command pool, buffer/memory
allocation, `vkCmdFillBuffer`, queue submit/wait and lease-driven destruction.
The test passes on the local ICD and skips boundedly when no physical device is
available. It is headless and uses zero-fill deliberately; no AC6 mesh/texture
upload, descriptor, render pass, swapchain or presentation is claimed.

The complete suite is 51/51. See
`reports/cycle-692-vulkan-headless-backend.md` for hashes and exact scope.

## NTXR image upload (cycle 693, 2026-08-03)

The Vulkan backend now uploads decoded NTXR RGBA8 pixels through a
host-visible staging buffer into an optimal 2D image, records both layout
transitions and `vkCmdCopyBufferToImage`, waits for queue completion and frees
the image/staging allocations. The backend smoke covers the 4×4 image path.

This is an upload proof only: BC1/BC3 native views, descriptors, render target,
mesh submission, swapchain and presentation remain open. See
`reports/cycle-693-vulkan-ntxr-upload.md`.

## NTXR descriptor binding (cycle 694, 2026-08-03)

The uploaded image now gets a real image view, sampler, descriptor-set layout,
bounded pool, allocated set and `vkUpdateDescriptorSets` write after the
shader-read transition; destruction frees the descriptor before the image/view.
The headless backend test covers this sequence and the full suite remains
51/51. See `reports/cycle-694-vulkan-ntxr-descriptor.md`.

No render pass/pipeline/mesh/swapchain is claimed yet; the Mission 2 retail
route remains unresolved.

## Headless render target (cycle 695, 2026-08-03)

The backend now creates an 8×8 color image/view, one-attachment render pass and
framebuffer, records a deterministic clear and waits for completion before
dependency-ordered release. The full suite remains 51/51. See
`reports/cycle-695-vulkan-headless-render-target.md`.

This is still clear-only: no AC6 shader pipeline, mesh upload, draw, readback,
swapchain or presentation is claimed.

## Vulkan upload-readiness gate (cycle 696, 2026-08-03)

The backend now rejects descriptor allocation before a successful image upload
and rejects duplicate uploads after the `SHADER_READ_ONLY_OPTIMAL` transition.
The final native CTest is 51/51; exact hashes are in
`reports/cycle-696-vulkan-upload-readiness.md`.

## Vulkan readback and bounded Mission 2 candidate (cycle 697, 2026-08-03)

`VulkanCampaignBackend::readback_render_target` now copies a completed RGBA8
target through a host-visible staging buffer, with explicit image barriers and
queue completion before the bytes are returned. The backend smoke clears an
8×8 target red and verifies `[255,0,0,255]` at the first pixel. This closes a
headless clear→readback contract; it does not claim a shader, mesh draw,
swapchain or presentation path. See
`reports/cycle-697-vulkan-readback-entry10.md`.

The PAC diagnostic is now index-generic (`ac6-entry-diagnostic`, while the
historical `ac6-entry9-diagnostic` target remains available). On the local PAL
corpus, static selector 2 resolves to DPL id 10 and DATA.TBL entry 10. A
bounded extraction of that record decodes to a 10,232,304-byte FHM containing
112 recursive rows, a 27-element MDLP child, 40 NTXR, 71 NDXR and 127 MATE
records. This is a useful Mission 2 resource candidate, not an interactive
retail route or completion claim: the available save profile is still empty.

## First generic Vulkan draw (cycle 698, 2026-08-03)

The backend now accepts caller-owned vertex/fragment SPIR-V, creates a
descriptor-free pipeline against a headless target render pass, uploads a
three-vertex buffer, issues `vkCmdDraw` and reuses the cycle-697 readback. The
fixture renders a green center pixel on an 8×8 target. CTest remains 51/51.
This proves the native queue/pipeline/vertex/draw/readback seam only; it is not
Xenos shader parity, AC6 mesh rendering or presentation. See
`reports/cycle-698-vulkan-triangle-draw.md`.

## Textured Vulkan draw (cycle 699, 2026-08-03)

The generic pipeline can now consume an uploaded texture descriptor: a
set-0/binding-0 sampler, position/UV vertex layout and `draw_textured_triangle`
are checked by sampling a uniform 4×4 RGBA8 image. The center pixel reads back
as `[127,127,127,127]`; CTest remains 51/51. This closes the host upload→image
layout→descriptor→fragment sample path, but the backend still has no qualified
Xenos shader, native BC1/BC3 block upload, mip chain, AC6 mesh layout or
presentation. See `reports/cycle-699-vulkan-textured-draw.md`.

## Native BC1/BC3 image upload (cycle 700, 2026-08-03)

`NtxrDecodedTexture::compressed_blocks` and the Vulkan upload path now preserve
qualified BC1/BC3 block images as `VK_FORMAT_BC1_RGBA_UNORM_BLOCK` and
`VK_FORMAT_BC3_UNORM_BLOCK`. A 4×4 red BC1 block and opaque green BC3 block are
sampled through the descriptor pipeline and read back exactly. CTest remains
51/51. The current NTXR decoder still emits RGBA8 and does not populate these
blocks from Xenos tiled/endian payloads, so this is a backend format seam, not
yet AC6 texture parity. See `reports/cycle-700-vulkan-bc1-bc3-upload.md`.

## Xenos block preservation and Mission 2 manifest (cycle 701, 2026-08-03)

The NTXR decoder now preserves linearized, endian-corrected BC1/BC3 blocks in
addition to RGBA8. The 128×128 BC3 fixture checks the native block prefix and
the decoded pixel, closing the handoff from Xenos tiled/endian decoding to the
cycle-700 native Vulkan block upload. The PAL manifest records selector 2 → DPL
10 → DATA.TBL[10] and its bounded FHM/MDLP structure, while explicitly keeping
interactive runtime and save qualification false. See
`reports/cycle-701-xenos-block-preservation-mission2-manifest.md`.

## File-backed PAL campaign pipeline (cycle 702, 2026-08-03)

Le runtime accepte maintenant des `CampaignPacBankSource` file-backed tout en
conservant la transaction de sélection. Le test retail optionnel valide le
`DATA.TBL` PAL (926 entrées, deux banques), lit exactement les tranches de
`DATA00.PAC` pour selector 1 → DPL 9 → entry 9 puis selector 2 → DPL 10 → entry
10, et traverse le même contrat loadout/objectifs/completion. Les deux lectures
sont bornées et `DATA01.PAC` n’est pas touché; sans `AC6_ASSET_ROOT`, CTest
saute proprement le test. CTest complet avec le corpus local : 52/52.

Cela ferme la preuve de plomberie physique sur payloads PAL réels, pas la
qualification interactive du retail : Mission 1 jouable, HUD/monde, sauvegarde
non vide et déverrouillage retail de Mission 2 restent ouverts. Voir
`reports/cycle-702-file-backed-retail-campaign-pipeline.md`.

## Chaîne de mips Vulkan générique (cycle 703, 2026-08-03)

`NtxrDecodedTexture` porte maintenant une chaîne facultative de niveaux, et le
backend AC6-owned valide les extents halved, les tailles RGBA8/BC1/BC3, puis
copie tous les niveaux dans une image Vulkan multi-mip avec transitions et vue
complètes. La fixture 8×8→1×1 rejette un niveau incohérent, réside quatre
niveaux et échantillonne le descripteur. CTest avec le corpus local reste
52/52.

Le décodeur NTXR n’attribue pas encore de mips aux payloads retail : aucun
champ Xenos n’est renommé ni supposé. Cette étape ferme le transport générique,
pas la parité texture AC6, les avions blancs, le monde gameplay noir ou la
présentation Vulkan. Voir `reports/cycle-703-vulkan-mip-chain-contract.md`.

## Contrat mesh NDXR sur le corpus PAL (cycle 704, 2026-08-03)

`CampaignMesh` convertit désormais les positions/UV/indices déjà décodés par
NDXR en flux portable, conserve l’identité texture et les `primitive_flags`
neutres, et rejette les incohérences de compte, indices hors bornes, UV
manquants et flottants non finis. Le test file-backed traverse les payloads
réels des routes 9 et 10 : 42 meshes (42 texturés) pour selector 1, 8 (8
texturés) pour selector 2. CTest complet : 53/53.

Cette preuve porte sur la préparation CPU et la métadonnée retail seulement;
la topologie Xenos, le vertex input Vulkan, le shader AC6, la profondeur et la
présentation restent ouverts. Voir
`reports/cycle-704-ndxr-native-mesh-contract.md`.

## Draw mesh indexé Vulkan (cycle 705, 2026-08-03)

Le backend AC6-owned sait maintenant uploader séparément vertices et indices,
valider un triangle-list explicite, appeler `vkCmdDrawIndexed` et relire un
quad texturé. Les indices hors bornes et les listes non triangulaires sont
rejetés. Le seam réutilise le shader `position.xy + uv` déjà qualifié; il ne
renomme ni le `z` NDXR ni les `primitive_flags`. CTest avec le corpus PAL :
53/53. Voir `reports/cycle-705-vulkan-indexed-mesh-draw.md`.

## Vertex input mesh Vulkan xyz+uv (cycle 706, 2026-08-03)

Une fixture SPIR-V glslang consomme maintenant `vec3 position` et `vec2 UV`;
`create_mesh_pipeline` configure un stride 20, un attribut RGB32 à l’offset 0
et les UV à l’offset 12. Le quad indexé du cycle 705 passe avec ce shader et
le `z` est réellement consommé par `gl_Position`. CTest reste 53/53.

Ce layout est une preuve Vulkan synthétique et non une qualification des
offsets Xenos/NDXR. Primitive flags, matrices, profondeur, shaders AC6 et
présentation restent ouverts. Voir
`reports/cycle-706-vulkan-mesh-vertex-layout.md`.

## Adaptateur CampaignMesh vers Vulkan (cycle 707, 2026-08-03)

`draw_campaign_mesh` raccorde directement le contrat CPU NDXR au layout Vulkan
xyz+UV et au draw indexé. La fixture exécute 4 vertices/6 indices via staging,
`vkCmdDrawIndexed` et readback texturé; CTest reste 53/53.

Le raccord ne projette pas les coordonnées, ne choisit pas de texture selon
MATE et ne réinterprète pas les `primitive_flags`. Caméra/matrices, profondeur,
shader AC6 et présentation restent ouverts. Voir
`reports/cycle-707-campaign-mesh-vulkan-adapter.md`.

## Contrat caméra/matrices native (cycle 708, 2026-08-03)

`camera_projection` construit désormais une view row-major à vecteurs-colonnes
à partir du `TcamCameraState` déjà borné, puis une perspective Vulkan à
profondeur `[0,1]`. La projection d'un `CampaignMesh` conserve le `w` clip,
les UV, indices, texture id et `primitive_flags`, tout en rejetant les
entrées non finies, derrière le plan proche ou hors bornes. La fixture couvre
identité, translation et profondeur ; CTest PAL complet : 54/54.

Cette convention est explicitement native et réutilise la base XYZ déjà
présente dans le shell SDL. Elle ne qualifie pas l'ordre de rotation Xenos,
le packing des registres, le clipping, les matériaux MATE, la depth target ou
la présentation swapchain. Voir
`reports/cycle-708-camera-projection-contract.md`.

## Pipeline clip-space et depth Vulkan (cycle 709, 2026-08-03)

Le backend accepte maintenant le résultat `CampaignProjectedMesh` avec un
vertex input `vec4 position + vec2 UV`, conserve `w` dans `gl_Position` et
réutilise le draw indexé borné. Une variante `create_depth_render_target`
ajoute un attachment `VK_FORMAT_D32_SFLOAT`, clear à 1.0 et comparaison
`LESS_OR_EQUAL` avec écriture depth. La fixture passe sur target couleur puis
couleur+depth avec readback central `[127,127,127,127]`; CTest PAL complet :
54/54.

Ce n'est pas encore une passe multi-mesh observable : chaque draw efface la
render pass. Le swapchain, le batch de scène, le shader Xenos/AC6 et le
mapping MATE→NTXR restent ouverts. Voir
`reports/cycle-709-vulkan-clip-depth-seam.md`.

## Batch depth et audit MATE/NDXR retail (cycle 710, 2026-08-03)

`draw_projected_mesh_batch` rébase plusieurs meshes clip-space et les soumet
dans une seule passe. La fixture depth sépare au pixel central un triangle
lointain rouge d'un triangle proche vert, ce qui ferme la preuve d'occlusion
multi-mesh sur la cible `D32_SFLOAT`.

Le gate PAL ajoute l'audit MATE/NDXR/NTXR : entry 9 expose 42 meshes
texturés, 1 885 correspondances first-texture et 1 021 bindings acceptés par
le résolveur natif après décodage BC1/BC3 ; entry 10 expose 8 meshes, 294
correspondances et 85 bindings acceptés. Le shader fourni au résolveur est
encore synthétique (UV/fetch/mip bornés), et ces variantes ne constituent pas
une sélection runtime unique. CTest complet : 54/54. Voir
`reports/cycle-710-vulkan-batched-depth-and-retail-material-audit.md`.

## Garde d'identité upload matériau (cycle 711, 2026-08-03)

`upload_material_texture` refuse maintenant tout NTXR dont le GIDX, les
dimensions, le format BC1/BC3 ou le nombre de mips résidents ne correspond pas
au `VulkanMaterialBinding`. La fixture accepte le binding BC3 cohérent,
rejette un GIDX différent et réutilise le chemin d'upload/descriptor existant.
Le shader retail et la sélection unique par objet restent ouverts. Voir
`reports/cycle-711-vulkan-material-upload-identity.md`.

## Contrat shader retail et frame Vulkan native (cycle 712, 2026-08-03)

Le premier contrat shader statique retenu est maintenant exprimé par
`ac6_d5b4_shader_contract` : vertex `A1863AF658456A14`, pixel
`D5B4F4A878949938`, fetch `tf0`, UV interpolées à l'offset 6 et mip minimal 0.
`draw_campaign_vulkan_frame` refuse un contrat divergent, lie le
`VulkanMaterialBinding` au NTXR par GIDX/format/extent/mips, soumet un pipeline
clip-space et un batch de meshes projetés, puis libère les handles temporaires.
La fixture vérifie le readback texturé et le rejet d'un hash pixel nul; CTest
PAL complet : 54/54.

Cette preuve est un raccord natif déterministe, pas une parité SPIR-V/Xenos :
les octets SPIR-V restent fournis par l'appelant. La présentation swapchain,
le monde/HUD de Mission 1, la sélection runtime unique et les gates de
sauvegarde/Mission 2 restent ouverts. Voir
`reports/cycle-712-vulkan-retail-shader-frame-seam.md`.

## Frame Vulkan persistante (cycle 713, 2026-08-03)

Le backend expose maintenant des cibles couleur et profondeur persistantes.
Elles utilisent `LOAD`, exigent un clear initial explicite par transitions
d'image et refusent tout draw avant cette initialisation. Deux soumissions
séparées sont observées simultanément dans le readback, puis la même preuve
est répétée avec depth. Les cibles historiques restent en `CLEAR`; CTest PAL
complet : 54/54. La surface/swapchain, la synchronisation multi-frame, le HUD
et le gameplay Mission 1 restent ouverts. Voir
`reports/cycle-713-vulkan-persistent-frame.md`.

## Catalogue générique de contrats shader (cycle 714, 2026-08-03)

Le renderer ne contient plus de branche spécifique D5B4 :
`draw_campaign_vulkan_frame` exige un catalogue de contrats qualifiés fourni
par le caller. Le premier catalogue contient le shader
`A1863AF658456A14`/`D5B4F4A878949938`; le gate retail l'emploie désormais pour
les parcours MATE→NDXR→NTXR réels. Les comptes restent 1 021 bindings entry 9
et 85 entry 10. Mission 2 pourra enregistrer un contrat supplémentaire sans
modifier le backend. La sélection runtime unique, la présentation et le HUD
restent ouverts. Voir `reports/cycle-714-generic-shader-catalog.md`.

## Frontière surface/swapchain Vulkan (cycle 715, 2026-08-03)

`create_with_extensions` sélectionne désormais une queue présentable quand
`VK_EXT_headless_surface` est demandé; le backend possède le chemin surface →
swapchain → acquire → copie depuis une cible persistante → `vkQueuePresentKHR`.
La surface headless locale est créée, mais le driver retourne
`VK_ERROR_INITIALIZATION_FAILED` lors du swapchain; le test consigne un skip
contrôlé et ne revendique aucune présentation observée. CTest PAL complet :
54/54. Voir `reports/cycle-715-vulkan-headless-presentation-boundary.md`.

## Contrat HUD/contrôles dans la frame native (cycle 716, 2026-08-03)

`CampaignHudInputState` relie la normalisation XInput, les actions logiques et
les edges; `build_campaign_hud_frame` conserve route DPL/DATA.TBL, loadout et
objectifs. La surcharge de `build_campaign_vulkan_frame` attache ce record à
la frame native. Les fixtures couvrent A/gun, B/missile, libération et
progression; CTest PAL complet : 55/55. Le rendu des glyphes/reticules, la
présentation et Mission 1 interactive restent ouverts. Voir
`reports/cycle-716-native-hud-input-frame.md`.

## Overlay HUD natif sur frame persistante (cycle 717, 2026-08-03)

`draw_campaign_hud_overlay` transforme maintenant le `CampaignHudFrame` en
rectangles clip-space sur une cible persistante déjà initialisée. Le backend
refuse une cible non persistante, non rendue, un render pass divergent ou un
pipeline texturé. La fixture conserve les deux meshes précédents et observe
des pixels verts d'overlay au readback; CTest PAL complet : 55/55.

Cette preuve ferme le seam géométrique HUD→Vulkan, pas le layout/couleurs des
glyphes retail ni la présentation swapchain. Les sauvegardes, le gameplay
Mission 1 et le déverrouillage Mission 2 restent ouverts. Voir
`reports/cycle-717-native-hud-overlay.md`.

## Surface SDL vers présentation Vulkan native (cycle 718, 2026-08-03)

`create_with_surface` accepte une factory de surface opaque : l'instance est
créée par le backend, SDL crée la surface contre cette instance, puis la
sélection de device/queue tient compte de la capacité de présentation. La
surface est consommée par `create_presentation_target`, avec nettoyage des
chemins d'erreur; le headless reste disponible. Le smoke SDL3 exerce ce
contrat, mais `SDL_VIDEODRIVER=dummy` ne fournit aucune fenêtre Vulkan et
retourne un skip contrôlé. CTest PAL : 56/56.

Ce raccord ne branche pas encore le shell SDL historique ni les meshes réels
de Mission 1 sur la swapchain. Le monde noir, le HUD retail, la sauvegarde et
Mission 2 restent les prochaines frontières. Voir
`reports/cycle-718-sdl-vulkan-presentation-seam.md`.

## Soumission native monde + HUD (cycle 719, 2026-08-03)

`draw_campaign_vulkan_frame_with_hud` consomme désormais la frame campagne
qualifiée, soumet la batch texturée puis le HUD attaché sur une cible
persistent et le même render pass. La fixture lit l'overlay après la frame et
conserve le chemin direct de garde; les tests Vulkan ciblés passent. Cette
preuve ne revendique ni fenêtre présentée, ni parité HUD, ni gameplay Mission
1/2. Voir `reports/cycle-719-native-frame-hud-submit.md`.

## Asset retail réel vers frame Vulkan (cycle 720, 2026-08-03)

Le module `campaign_retail_frame` joint maintenant FHM/MDLP, NDXR, MATE et
NTXR sans fabriquer d'identifiant. La règle de topologie déjà exécutée par
le shell SDL convertit les triangle strips et leurs restart `0xffff` en
listes triangle; entry 9 et entry 10 donnent chacune un mesh 4 sommets/6
indices et le GIDX réel `268444181`. Le backend accepte aussi le payload
décodeur réel qui contient simultanément BC et RGBA.

Le test Vulkan offscreen rasterise les deux ressources avec leur texture et
leur HUD (`scene_changed=8568`, `hud_green=342` pour chaque entrée); CTest
PAL complet : 58/58. Le cadrage est volontairement AABB diagnostic : TCAM,
swapchain, STANDBY, gameplay, sauvegarde et Mission 2 restent ouverts. Voir
`reports/cycle-720-retail-frame-asset-to-vulkan.md`.

## CUT/TCAM réel vers projection Vulkan (cycle 721, 2026-08-03)

`campaign_scene_frame` rejoue maintenant le groupe Scene de l'entrée 9,
vérifie `CutStart → FrameStart(1) → MoveCamera`, résout
`Scene/dd01_01a/dd01_01a_01/Tcam__c01.mop` et joint 16 transforms
`Rigid/AnimRigid` du même frame. `transform_campaign_mesh` applique la
convention XYZ déjà qualifiée par le shell SDL. La projection expose le signe
de profondeur explicitement : le premier frame PAL est en profondeur locale
négative et le test fournit `camera_forward_sign=-1`, sans profondeur absolue.

La frame Vulkan consomme donc un mesh/material retail, la caméra TCAM et le HUD
sur le même chemin générique. À 128×128, le premier mesh est sous-pixellaire
(`scene_changed=0`) : le test ne revendique pas encore un monde non noir à
résolution normale. Entry 10 conserve son smoke AABB car il ne contient pas de
groupe Scene/TCAM. Les tests ciblés passent 5/5; STANDBY, swapchain réelle,
contrôles en vol, sauvegarde et Mission 2 restent ouverts. Voir
`reports/cycle-721-tcam-cut-to-vulkan.md`.

## STANDBY runtime vers frame Vulkan (cycle 722, 2026-08-03)

`CampaignRuntimeState` expose maintenant la phase native
`idle → loadout → briefing → standby → active → complete`. L’entrée STANDBY
est distincte du statut de progression `briefing`; le passage vers `active`
exige `launch_campaign_runtime_standby()` avec l’edge A qualifié (slot
d’action 0). Un appel sans edge et un démarrage direct depuis STANDBY sont
rejetés. `CampaignVulkanFrame` transporte la phase runtime.

La fixture Vulkan retail construit désormais un manifest depuis `DATA.TBL`,
charge les entrées 9/10 via `CampaignRuntimeState`, équipe le loadout,
soumet la frame STANDBY et son HUD sur une cible séparée, injecte A, puis
soumet la frame active avec le même backend. Selector 1 joint toujours
`Tcam__c01.mop` et 16 transforms; son mesh initial reste sous-pixellaire à
128×128 (`scene_changed=0`). Le HUD STANDBY et actif lit 300 pixels verts;
selector 2 produit `scene_changed=8568` par le même pipeline. Tests ciblés
6/6. Vol interactif, visibilité monde à résolution normale, swapchain,
sauvegarde et Mission 2 restent ouverts. Voir
`reports/cycle-722-standby-runtime-to-vulkan.md`.

## Batch monde Scene et géométrie sans diffuse (cycle 723, 2026-08-03)

Le convertisseur retail corrige désormais les triangle strips : après retrait
des restart `0xffff`, le `index_count` borné est recalculé avant le contrat
`CampaignMesh`. Le join nommé expose tous les polygones qualifiés par
`Scene asset key → NDXR → MATE → NTXR`, au lieu du seul premier polygone.

Les ressources Scene sans diffuse MATE/NTXR qualifié restent disponibles sous
forme de géométrie-only, sans identifiant de texture inventé. Le backend Vulkan
ajoute un batch clip-space sans descriptor; il partage la cible persistante et
le pipeline seam sans confondre cette voie avec une preuve depth ou shader Xenos.

La fixture PAL, après le vrai manifest/runtime STANDBY puis edge A, joint 115
parts texturés (les avions `r_f16c`/`r_f18f`) et 91 parts géométriques solides
du premier Scene frame. À 128×128, le readback actif passe à
`scene_changed=3`, avec 303 pixels verts HUD; selector 2 reste générique et
produit `scene_changed=166`. CTest ciblé : 4/4; CTest PAL complet : 59/59.

Ce cycle ferme la soumission monde multi-polygones headless, pas la fenêtre
swapchain, le vol interactif, la parité des matériaux blancs, la sauvegarde ou
le déverrouillage Mission 2. Voir
`reports/cycle-723-world-batch-and-solid-fallback.md`.

## Contrat de vol natif et snapshot de progression (cycle 724, 2026-08-04)

Le nouveau module `campaign_flight` reçoit des axes normalisés
pitch/roll/yaw/throttle et un frein. Il rejette les valeurs non finies et les
pas hors borne, avance une pose locale bornée, puis la réapplique à la caméra
TCAM PAL qualifiée. La fixture réelle passe STANDBY puis l'edge A et observe
`flight_changed=1` après une commande native. Cette preuve ferme le raccord
contrôle→état→projection; elle ne prétend pas avoir retrouvé les équations de
physique retail ni une boucle SDL interactive.

`campaign_save` définit le format versionné big-endian `AC6S` (version 1,
missions terminées et masques d'objectifs), avec validation de magic, version,
troncature, compte et octets résiduels. `restore_campaign_progression` vérifie
la forme, les IDs, les doublons, les bits d'objectifs et la complétude avant une
mutation transactionnelle. La fixture termine les deux objectifs de Mission 1,
encode/décode 28 octets, restaure un runtime neuf et vérifie l'état disponible
de Mission 2 (`mission2_restored=1`), puis sélectionne l'entrée 10 avec le
même manifest/PAC loader/frontend.

Résultat PAL : entry 9 produit 115 parties texturées, 91 géométriques solides,
`scene_changed=3`, 303 pixels HUD verts, 16 transforms TCAM et le chemin
`Scene/dd01_01a/dd01_01a_01/Tcam__c01.mop`; entry 10 produit
`scene_changed=166` et 300 pixels HUD verts. CTest ciblé : 6/6 en 2,31 s;
CTest PAL complet : 61/61 en 63,82 s.

Ce cycle ne ferme pas la présentation sur une swapchain non-dummy, la I/O d'un
fichier de sauvegarde retail ou natif, la parité des matériaux blancs, ni le
vol/la complétion de Mission 2. Aucun asset retail n'est ajouté au dépôt. Voir
`reports/cycle-724-flight-controls-and-save-restore.md`.

## Sauvegarde `AC6S` persistante et reprise PAL (cycle 725, 2026-08-04)

`campaign_save_io` ajoute un contrat de fichier borné indépendant de SDL et de
Vulkan. Le writer ferme un voisin `.tmp`, puis le remplace par `rename` POSIX
ou `MoveFileExW(REPLACE_EXISTING|WRITE_THROUGH)` Windows. Le reader limite le
payload à 16 MiB et distingue chemin invalide, ouverture, lecture, taille et
décodage sans allouer un fichier non borné. La durabilité après panne et les
verrous interprocessus restent explicitement hors preuve.

La fixture PAL termine Mission 1, écrit son snapshot `AC6S` de 28 octets,
le relit depuis disque, restaure un runtime neuf et vérifie le déverrouillage
de Mission 2 (`save_file_restored=1`, `mission2_restored=1`). Entry 10 est
ensuite sélectionnée par le même manifest/PAC loader/frontend; aucun fallback
renderer par mission n'est ajouté. CTest ciblé : 7/7 en 2,37 s; CTest PAL
complet : 62/62 en 63,58 s.

Ce cycle ferme la reprise native fichier, pas la compatibilité d'un fichier de
sauvegarde retail, la swapchain non-dummy, le vol interactif ou la complétion
de Mission 2. Aucun asset retail n'est ajouté au dépôt. Voir
`reports/cycle-725-file-backed-save-checkpoint.md`.

## Frame campagne PAL présentée par SDL/Vulkan (cycle 726, 2026-08-04)

Le smoke `campaign_vulkan_sdl_present_tests` réutilise le manifest, le loader
PAC et la phase native `STANDBY → A → active`. Il crée une fenêtre SDL Vulkan,
transfère sa `VkSurfaceKHR` au backend AC6, construit la swapchain 640×360,
soumet le renderable texturé entry 9 et le HUD, puis appelle
`present_render_target`. Sous Xvfb/X11, le résultat est
`vulkan_campaign_sdl_presented=1 mission=1 hud=1`, avec trois groupes Scene,
`scene_changed=4439`, `world_changed=11` (dont `textured_changed=1`) et
`hud_green=4439` au readback avant présentation. Le groupe Scene 0, la caméra
`Tcam__c01.mop` et les transforms
frame-locales `Rigid/AnimRigid` sont donc bien dans le chemin présenté. Leur
enveloppe clip observée est `x=-0.612867..-0.336477`,
`y=0.228415..0.472099`; les seuls 11 pixels monde signalent désormais une
frontière topologie/alpha/profondeur/matériaux, pas une absence de caméra.

Le CTest PAL complet passe 63/63 avec un skip contrôlé du nouveau test sous
`SDL_VIDEODRIVER=dummy`; le ciblé est 8/8 avec le même skip. Cette preuve ferme
la surface/swapchain et une frame campagne présentée. Les 11 pixels de monde
hors HUD montrent toutefois la frontière restante de visibilité (cadrage,
échelle, profondeur et matériaux), ainsi que la boucle SDL d'axes de vol.
Mission 2 reste sélectionnable mais non présentée/volée. Aucun asset retail
n'est ajouté au dépôt. Voir
`reports/cycle-726-pal-campaign-sdl-vulkan-presentation.md`.

## Axes SDL, reprise AC6S et présentation Mission 2 (cycle 727, 2026-08-04)

Le contrat `CampaignFlightHostAxes` adapte maintenant les axes signés 16 bits
de l'hôte en valeurs bornées `[-1,1]`, avec traitement explicite de `-32768`.
Le smoke SDL injecte quatre événements gamepad, les collecte par axe, les
normalise puis avance huit pas de simulation native. La reprojection avec la
pose TCAM modifiée donne `flight_changed=1`; cette preuve est indépendante de
la physique retail et ne simule pas un état invité forcé.

La même fixture termine Mission 1, écrit puis relit un snapshot `AC6S` depuis
disque, restaure un runtime neuf et vérifie `mission2_restored=1`. Mission 2
est sélectionnée à partir de l'entrée 10 de `DATA.TBL` par le manifest et le
loader PAC déjà qualifiés, puis rendue et présentée sur la même swapchain avec
HUD (`mission2_presented=1`, `mission2_changed=6974`,
`mission2_hud_green=4428`). CTest ciblé : 8/8 avec un skip dummy contrôlé;
CTest PAL complet : 63/63 avec le même skip.

Le readback du mesh de vol texturé reste nul (`flight_world_pixels=0`) alors
que sa projection change et que le HUD est visible (`flight_pixels=4428`). Le
batch Scene principal reste positif (`world_changed=11`). Cette frontière est
donc maintenant localisée aux matériaux/alpha/topologie/profondeur et non à la
surface, à la swapchain ou au raccord de commande. La boucle physique SDL
persistante, la complétion de Mission 2 et la parité des avions blancs des
cutscenes restent ouvertes. Aucun asset retail n'est ajouté au dépôt. Voir
`reports/cycle-727-flight-axis-mission2-presented.md`.

## Dispatch générique des événements de progression (cycle 728, 2026-08-04)

`CampaignRuntimeEvent` et `apply_campaign_runtime_event()` forment désormais
le point de mutation commun pour les événements `objective_completed` et
`mission_completed`. Une complétion de mission prématurée est rejetée sans
mutation; les objectifs requis doivent être émis avant l'événement de mission.
La fixture SDL termine Mission 1 par ce dispatch, sans appel direct spécifique
à la mission. CTest ciblé : 4/4 avec un skip dummy contrôlé; CTest PAL complet :
63/63 avec le même skip.

Cette étape ne fabrique pas encore les événements depuis le monde, les
collisions ou une logique de mission retail. Mission 2 est présentée mais pas
volée/complétée, et `flight_world_pixels=0` demeure la frontière graphique.
Voir `reports/cycle-728-generic-runtime-events.md`.

## Couverture monde de la pose de vol (cycle 729, 2026-08-04)

La fixture SDL conserve tous les meshes géométriques Scene transformés par les
`Rigid/AnimRigid` du CUT, les reprojette avec la caméra issue des axes natifs
et les soumet au pipeline solide commun. Sous Xvfb, le résultat est
`flight_changed=1`, `flight_world_pixels=12` et `flight_pixels=4440`; la pose
atteint donc effectivement le raster hors HUD. Le batch texturé reste faible
(`textured_changed=1`) : cette preuve localise la suite aux contrats
matériaux/alpha/profondeur, sans introduire de LOD ou force flag.

Le dispatch générique termine toujours Mission 1, la reprise `AC6S` déverrouille
Mission 2 et sa frame est présentée par le même loader/backend. Mission 2 n'est
pas encore volée/complétée et les événements ne proviennent pas encore d'une
logique monde retail. CTest ciblé : 5/5 avec un skip dummy; CTest PAL complet :
63/63 avec le même skip. Voir
`reports/cycle-729-flight-world-coverage.md`.

## Capture X11 post-présentation (cycle 730, 2026-08-04)

Une pause de harness (`AC6_SCREENSHOT_HOLD_MS`) permet maintenant de capturer
la fenêtre après le retour de `vkQueuePresentKHR`. La capture
`/home/lavaulta/Pictures/screenshot-2026-08-04_02-59-11.png` est entièrement
noire (640×480, 307 200 pixels noirs), tandis que la même exécution conserve
`scene_changed=4439`, `hud_green=4439`, `flight_world_pixels=12` et
`mission2_changed=6974` au readback Vulkan. La soumission et le readback sont
donc validés, mais la composition/visibilité réelle dans la fenêtre X11 ne
l'est pas. Cette preuve négative devient la prochaine frontière graphique;
elle n'altère pas les contrats de campagne. Voir
`reports/cycle-730-x11-screencap-black.md`.

## Capture SDL/Vulkan visible (cycle 731, 2026-08-04)

La capture noire du cycle 730 provenait de `SDL_WINDOW_HIDDEN`, non d'une
absence de contenu Vulkan. Les deux smoke tests appellent désormais
`SDL_ShowWindow()` avant la création de surface; le backend préfère aussi le
format RGBA8 identique aux cibles natives pour la copie de présentation.
Après `vkQueuePresentKHR`, la capture
`/home/lavaulta/Pictures/screenshot-2026-08-04_03-06-00.png` montre l'avion,
le fond monde et le HUD vert dans la fenêtre 640×360. La sortie conserve
`scene_changed=4439`, `flight_world_pixels=12`, `mission2_presented=1` et
`mission2_changed=6974`. CTest ciblé : 4/4 avec un skip dummy; CTest PAL
complet : 63/63 avec le même skip. La faible couverture texturée
(`textured_changed=1`) et la parité des cutscenes restent ouvertes. Voir
`reports/cycle-731-visible-sdl-vulkan-screencap.md`.

## Frontière graphique requalifiée (cycle 732, 2026-08-04)

La capture visible du cycle 731 n'est pas une frame de gameplay acceptable :
le HUD vert est diagnostique, l'avion provient d'un mesh Scene de cinématique,
Mission 2 est ajustée artificiellement au clip et le groupe chargé ne fournit
ni terrain ni skybox. Un clipping triangle-par-triangle au plan proche a été
ajouté sans changer le contrat strict historique; le test dédié passe, mais
les métriques restent `world_changed=11`, `textured_changed=1` et
`flight_world_pixels=12`. L'inventaire metadata-only de l'entry 9 trouve
292 NDXR et quatre candidats `mapobj` statiques. La prochaine étape est de les
rejoindre aux transforms/batches runtime, puis de qualifier sky/cloud et la
pose de gameplay avant toute nouvelle présentation. Voir
`reports/cycle-732-graphical-frontier.md`.

Le harness SDL ne retient plus Mission 2 implicitement : une capture doit
demander explicitement `AC6_SCREENSHOT_STAGE=mission1`, `flight` ou
`mission2-diagnostic`; ce dernier reste un `fit_mesh_to_clip` de diagnostic et
ne constitue pas une orientation de vol.

## Handoff de redémarrage (cycle 733, 2026-08-04)

Le handoff de reprise vers une Mission 01 native jouable 1:1 est consigné dans
`reports/cycle-733-session-restart-handoff.md` et son JSON associé. Il conserve
la frontière exacte, les commandes de validation, les assets qualifiés, les
processus, le prompt de reprise et le nouveau `/goal`. Les anciens pipelines
AC6 `ac6-scene-shell` et Xvfb `:98` ont été terminés proprement après
autorisation; le Xvfb `:106` de Pharaoh n'a pas été touché.

## Batch mapobj Mission 01 qualifié (cycle 760, 2026-08-04)

Les quatre assets `mapobj_m01_l_brg1_n`, `brg1_b`, `brg2_n` et `brg2_b` de
l'entry 9 passent désormais le join MATE→NDXR→NTXR et produisent respectivement
1, 3, 1 et 3 parties rendables. Le support NTXR ajouté reste fail-closed sur le
profil exact de 24 mots, qualifié par les fetches runtime BC3 256×256 tiled.

Les draws gameplay exécutés identifient seulement `brg2_n` (frame GPU 9664,
draw 468) et `brg1_n` (frame 9665, draw 514). Chaque draw lit le vertex range de
son NDXR nommé et la base physique de son NTXR via une image/view Vulkan non
nulle. Le microcode `C1EE3147DFD5E624` qualifie l'opération object-to-clip
`x*c218+y*c219+z*c220+c221`; le renderer natif l'expose comme seam générique,
sans TCAM, fit-to-clip ou rotation inventée.

Sous Xvfb/X11, deux exécutions reproductibles donnent
`flight_world_pixels_before_environment=12`,
`qualified_environment_pixels=129` et `flight_world_pixels=141`, tous mesurés
avant le HUD. Build réussi, tests discriminants 3/3, CTest PAL complet 63/63
avec le skip SDL dummy prévu. Terrain, sky/cloud, avion joueur/LOD, caméra de
vol, variantes `_b`, avion blanc et premier stage noir restent ouverts. Le run
runtime source avait `ac6_unlock_fps=true`; il ne vaut donc pas baseline timing
stock. Voir `reports/cycle-760-qualified-entry9-mapobj-batch.md`.

## Terrain, sky/cloud et caméra gameplay stock (cycle 774, 2026-08-04)

Une route fraîche Mission 01 sous le backend Vulkan vendored confirme
`ac6_unlock_fps=false` et capture une frame GPU complète après le HUD et les
entrées de vol. Ses 1 340 draws relient par taille+XXH3 les NDXR
`mapparts_m01_*` de l'entry 119, les huit NDXR du paquet sky/cloud
`entry119/022_FHM`, leurs textures NTXR effectivement liées et les matrices
c218–c221 exécutées. Terrain, sky/cloud et caméra de vol sont donc identifiés
sans CUT, `fit_mesh`, skybox ou transform fabriqué.

L'entry 9 identifie `o_f16c_lod1` à `lod4` et `r_f16c_dd`, mais aucun vertex
hash exact ne joint encore la frame ; l'avion joueur/LOD runtime reste ouvert.
Le draw d'avion blanc a un fetch type 2 lisible, un format/image/view Vulkan
valides et un sampler réel : la classe invalid/null est exclue, les classes
format/swizzle, shader/constantes, cible et blend/lighting restent à séparer.
Le HUD est visible sur un monde noir et le premier étage noir n'est pas encore
localisé. Build runtime/reconstruction réussi, CTest PAL 63/63. Voir
`reports/cycle-774-stock-gameplay-terrain-sky-camera.md`.
### Cycle 795 — route d’amorçage explicite non concluante

Le binaire oracle qualifié et le profil cycle 782 ont été relancés avec
`user_data_root` explicite, audio SDL dummy et touches temporisées. L’intro
Project Aces atteint 105 frames, mais la route ne produit ni `ac6-save-outer`
ni `type28=30`; aucune nouvelle preuve de Mission 01 n’est ajoutée.

## Export framebuffer natif (cycle 841, 2026-08-04)

`NativeRenderTarget::write_ppm` expose un export PPM P6 borné au framebuffer
de vérification. Le harnais l'active seulement avec
`AC6_NATIVE_FRAME_DUMP=/path/frame.ppm`; `ac6-native` ne l'appelle pas et ne
crée toujours pas de fenêtre SDL/Vulkan. La fixture 64×32 a produit
`reports/cycle-841-native-frame.ppm` et sa conversion PNG de consultation,
avec un test d'en-tête, de taille et de chemin vide. Cette image est
synthétique et ne qualifie pas une capture retail. Voir
`reports/cycle-841-native-frame-export.md`.

## Input conservé dans WorldFrame (cycle 842, 2026-08-04)

`WorldFrame` transporte désormais l'`InputFrame` consommé par chaque tick.
Une fixture vérifie une frame neutre, 120 ticks avec axes/throttle/boutons
non nuls sur deux replays, puis le retour aux cinq champs neutres. La pose,
le tick et l'input restent déterministes. Voir
`reports/cycle-842-native-input-frame-contract.md`.

## Mappings d'input déclaratifs (cycle 843, 2026-08-04)

`InputMappingDatabase` charge des masques de boutons et événements explicites
depuis un manifeste, avec rejet fail-closed des doublons, masques invalides,
actions inconnues et manifestes vides. Aucun mapping SDL/PAL n'est inventé dans
le produit. Voir `reports/cycle-843-declarative-input-mapping.md`.

## Dispatch scénario depuis mapping (cycle 844, 2026-08-04)

`MissionScenario::dispatch_buttons` résout un masque dans le manifeste puis
dispatch l'événement explicite au HSM; les masques absents sont rejetés. La
fixture couvre démarrage, pause, reprise et bouton inconnu sans branche
`mission_id`. Voir `reports/cycle-844-input-to-scenario-dispatch.md`.

## Fichier replay déterministe (cycle 845, 2026-08-04)

`ReplayLog` sérialise maintenant un format `AC6RPLY` versionné et borné. Le
chargement est atomique et rejette magic/version, troncature et octets
supplémentaires invalides. La fixture vérifie une reprise bit-à-bit et un
fichier corrompu. Voir `reports/cycle-845-replay-file-contract.md`.

## Fichier de sauvegarde progression (cycle 846, 2026-08-04)

`SaveStore` persiste les snapshots dans un format `AC6SAVE` versionné, borné
et déterministe. Le chargement atomique rejette snapshots invalides, doublons,
troncature et octets supplémentaires. Deux slots et une corruption sont
testés. Voir `reports/cycle-846-save-file-contract.md`.

## Écriture sauvegarde atomique (cycle 847, 2026-08-04)

`SaveStore::write_file` publie maintenant via temporaire puis renommage. Les
échecs nettoient le temporaire et ne publient aucun fichier partiel; la fixture
vérifie ce comportement avec un parent absent. Voir
`reports/cycle-847-atomic-save-write.md`.

## Frontend piloté par événements (cycle 848, 2026-08-04)

`FrontendController` consomme maintenant `StartMission` et `Abort` via le
mapping déclaratif. Une fixture traverse Title → New Game → Briefing → Hangar
→ Loading → Mission, puis revient à Title sur abort, sans choix codé de
`mission_id`. Voir `reports/cycle-848-frontend-event-route.md`.

## Adaptateur SDL3 séparé (cycle 849, 2026-08-04)

`ac6_platform` traduit les axes/boutons SDL3 vers `InputFrame` et événements
déclaratifs, avec inversion et throttle configurables. Le cœur reste sans
dépendance SDL3; la fixture synthétique couvre axe, throttle, bouton et
relâchement. La boucle d'événements, les périphériques réels et la swapchain
restent ouverts. Voir `reports/cycle-849-sdl-input-adapter.md`.

## Pompe d'événements SDL3 (cycle 850, 2026-08-04)

`SdlEventPump` possède le sous-système events, dépile `SDL_PollEvent`, relaie
les événements à l'adaptateur et transforme QUIT en drapeau de sortie. Une
fixture dummy initialise SDL, injecte QUIT et ferme proprement. La boucle
principale et la fenêtre restent à raccorder. Voir
`reports/cycle-850-sdl-event-pump.md`.

## Ownership fenêtre SDL3 (cycle 851, 2026-08-04)

`SdlWindow` possède création, affichage et destruction, avec choix explicite
du flag Vulkan. Le test dummy crée une fenêtre cachée non-Vulkan puis vérifie
la destruction avant SDL shutdown. La fenêtre produit et la surface/swapchain
Vulkan restent ouvertes. Voir `reports/cycle-851-sdl-window-ownership.md`.

## Seam surface Vulkan SDL3 (cycle 852, 2026-08-04)

`SdlVulkanSurface` encapsule les extensions SDL, la création/destruction d'une
`VkSurfaceKHR` et l'ordre de vie fenêtre→surface. Les rejets de fenêtre ou
instance invalides sont testés sous dummy; aucune surface fictive n'est
qualifiée. Voir `reports/cycle-852-sdl-vulkan-surface-seam.md`.

## Instance et surface Vulkan positives (cycle 853, 2026-08-04)

`VulkanInstance` valide les extensions SDL et possède l'instance Vulkan. Le
smoke Xvfb crée ensuite une fenêtre Vulkan et une `VkSurfaceKHR` réelle, puis
détruit dans l'ordre inverse. Sortie : `sdl_vulkan_surface=1 extensions=2`.
Physical device, queue, swapchain et présentation du monde restent ouverts.
Voir `reports/cycle-853-vulkan-instance-surface-positive.md`.

## Physical device et queue Vulkan (cycle 854, 2026-08-04)

`VulkanDevice` sélectionne une queue graphique et présentable, exige
`VK_KHR_swapchain`, crée le device logique et détruit après attente idle. Le
smoke Xvfb surface+device termine avec code 0; CTest dummy reste vert. Voir
`reports/cycle-854-vulkan-device-queue.md`.

## Swapchain Vulkan (cycle 855, 2026-08-04)

`VulkanSwapchain` qualifie capacités/formats/modes, choisit FIFO, borne
extent, crée images et image views et nettoie les créations partielles. Le
smoke Xvfb instance→surface→device→swapchain termine avec code 0; CTest dummy
reste vert. Render pass, submit/present et transfert du framebuffer restent
ouverts. Voir `reports/cycle-855-vulkan-swapchain.md`.

## Premier submit/present Vulkan (cycle 856, 2026-08-04)

`VulkanFramePresenter` acquiert, transitionne, clear, soumet et présente une
image de swapchain avec sémaphores/fence. Le smoke Xvfb termine avec code 0;
la couleur est une validation, pas une frame Mission 01. Le transfert du
`NativeRenderTarget` et la boucle `ac6-native` restent ouverts. Voir
`reports/cycle-856-vulkan-present-clear.md`.

## Upload framebuffer natif Vulkan (cycle 857, 2026-08-04)

`NativeRenderTarget::copy_rgba8` et `VulkanFramePresenter::present_frame`
ferment le chemin pixels→staging buffer→image swapchain→present, avec mise à
l'échelle bornée. Le smoke Xvfb présente une seconde frame issue d'une cible
native 64×32 et termine avec code 0. Les assets Mission 01 et l'appel depuis
`ac6-native` restent ouverts. Voir `reports/cycle-857-native-frame-upload.md`.

## Presenter raccordé à ac6-native (cycle 858, 2026-08-04)

`ac6-native --present-smoke` exécute désormais le chemin SDL/Vulkan complet et
présente une cible native clearée; le démarrage sans argument reste headless.
Les deux modes retournent 0 et CTest reste vert. La géométrie retail et le
chargement Mission 01 restent à brancher. Voir
`reports/cycle-858-ac6-native-present-smoke.md`.

## Orchestrateur MissionExecution (cycle 859, 2026-08-04)

`MissionExecution` centralise définition, assets, lancement, unités, joueur,
scénario et `WorldFrame`; les erreurs de lancement remettent la session à
zéro. La fixture couvre les assets qualifiés 9/119/165/199/210 et deux unités.
`ac6-native` doit encore charger les manifestes et bases géométriques réelles.
Voir `reports/cycle-859-mission-execution-orchestrator.md`.

## Manifeste runtime externe (cycle 860, 2026-08-04)

`MissionManifestLoader` résout et charge atomiquement catalog, assets et
lancements depuis des chemins relatifs externes. La fixture vérifie mission 1,
asset 9 et joueur 4097 sans asset embarqué. Les manifestes render/géométrie et
la consommation par `ac6-native` restent à faire. Voir
`reports/cycle-860-runtime-manifest-loader.md`.

## Couverture chemins render (cycle 861, 2026-08-04)

`MissionManifestPaths` et `render_valid()` couvrent désormais les dix bases
render externes (drawables, transforms, materials, textures, shaders, targets,
passes, resolves, buffers et définition). Une fixture vérifie la couverture
complète tout en conservant le manifeste runtime minimal. Le chargement de ces
bases et leur usage par `ac6-native` restent ouverts. Voir
`reports/cycle-861-render-manifest-paths.md`.

## Chargement atomique des bases render (cycle 862, 2026-08-04)

`MissionManifestLoader::load_render` charge les dix bases render dans des
temporaires et ne publie qu'après succès complet. `render_valid()` est exigé;
les bases appelantes restent intactes en cas d'échec. Un jeu de manifests
render qualifié persistant et la consommation par `ac6-native` restent ouverts.
Voir `reports/cycle-862-render-manifest-loader.md`.

## Validation manifeste par ac6-native (cycle 863, 2026-08-04)

`ac6-native --validate-manifest <path>` exige la couverture render puis charge
runtime et dix bases graphiques avec les loaders atomiques. Les chemins absents
échouent proprement; le mode reste développeur et n'embarque aucun asset.
MissionExecution, renderer et présentation de la géométrie restent à raccorder.
Voir `reports/cycle-863-native-manifest-validation.md`.

## Route manifeste complète ac6-native (cycle 864, 2026-08-04)

`--present-manifest <manifest> <mission_id>` charge les bases, vérifie/décode
les buffers, lance `MissionExecution`, rend `WorldFrame` via
`VulkanRenderer` et présente via SDL/Vulkan. Le chemin absent échoue en code 9;
la route positive attend volontairement les manifests/buffers retail locaux.
Voir `reports/cycle-864-native-present-manifest-route.md`.

## Loader render + géométrie atomique (cycle 865, 2026-08-04)

La surcharge `load_render(..., NativeGeometryDatabase&)` vérifie et décode tous
les buffers référencés dans des temporaires, puis publie bases et géométrie
ensemble. `ac6-native --present-manifest` l'utilise désormais. La preuve retail
positive attend toujours les manifests et bytes qualifiés locaux. Voir
`reports/cycle-865-render-loader-geometry-atomic.md`.
