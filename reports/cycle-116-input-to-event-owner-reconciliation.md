# AC6 cycle 116 — raccord de l'entrée logique vers le receveur événementiel

## Périmètre

Cette tranche réconcilie les dossiers statiques d'entrée et le dernier
receveur événementiel inspecté headlessly. Elle concerne le binaire Xbox 360
PAL `default.xex`, SHA-256
`acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde`.

L'analyse Ghidra est en lecture seule, avec `-noanalysis`; aucun fichier
XenonRecomp, projet Ghidra, état Xenia, fichier retail ou source générée n'a
été modifié. Aucune session Wine, Xenia, GUI ou humaine n'a été lancée.

## Chaîne qualifiée

Les éléments suivants forment une chaîne structurelle, sans donner de
sémantique de touche, d'axe, d'avion, d'arme ou de caméra :

```text
0x821CE088
  raw device words
      -> canonical state at 0x826EDB98 (stride 0xa0)
0x821BE268 / 0x821CDF08
  default logical masks and five-context update
      -> context logical slots
0x82215140
  digital masks -> aggregate word
0x82215210
  analog source words -> logical samples
0x82214F88
  current/previous -> pressed/released/repeat edges
      -> state+0xe4c/+0xe50/+0xe54
0x820DB500 (interior/thunk context)
  generic threshold/event adapter
0x8237E4C0
  event receiver state updates
```

Le statut `recompiler-generated` n'est pas utilisé comme preuve de cette
chaîne; les liens ci-dessus reposent sur les décompilations, les références et
les tests natifs déjà enregistrés.

## Frontière logique et analogique déjà fermée

- `0x821CE088` lit quatre slots de périphérique via `0x8233B470` et
  `0x8233B428`, puis écrit les états canoniques à `0x826EDB98`, stride
  `0xa0`. Les masques numériques et les six sources analogiques sont
  documentés dans `FUNCTION_821CE088_PLAYER_INPUT_REPORT.md` et
  `FUNCTION_821CE088_ANALOG_RECEIVER_REPORT.md`.
- `0x821BE268` installe les masques par défaut des slots logiques; le contexte
  de gameplay est mis à jour dans `0x821CDF08` avec cinq contextes. Aucun
  lecteur statique ne relie encore ces slots à une transformation d'avion.
- `0x82215140` projette les mots digitaux et le masque externe vers
  `state+0xe44`; `0x82215210` applique le chemin analogique et ses paramètres
  de zone morte/inversion.
- `0x82214F88` calcule `current & ~previous`, `previous & ~current` et un
  masque de répétition temporisé dans `state+0xe4c/+0xe50/+0xe54`, avec des
  compteurs flottants jusqu'à `state+0x115c`.
- Les seuls appels directs trouvés dans cette tranche restent internes à
  l'agrégateur: `0x82215470 -> 0x82215140`,
  `0x82215484 -> 0x82215210` et `0x822154A0 -> 0x82214F88`. L'entrée
  `0x82215418` est toujours atteinte par la table runtime `0x82080C40` plutôt
  que par un appel statique ordinaire.

## `0x8237E4C0` — contrat du receveur

La décompilation headless de `0x8237E4C0` établit les effets suivants lorsque
le receveur est dans l'état actif (`*receiver == 0` et `receiver[2] != 0`) :

- type d'événement `1`: copie `payload[1]` et `payload[2]` vers
  `receiver+0x124` et `receiver+0x128`;
- type `5`: utilise l'octet `payload+4` comme index de bit, pose ce bit dans
  une des zones `receiver+0x12c..+0x148` et le recopie en `receiver+0x14c`;
- type `6`: efface le même bit;
- les types `5` et `6` appellent tous deux la vtable du pointeur
  `receiver+0xdc`, slot `+0x2c`, puis les sorties runtime
  `0x82381570` ou `0x82381540`;
- les autres types peuvent appeler `0x82382E40` lorsque la paire de pointeurs
  `receiver+0xfc/+0x100` est cohérente.

Le propriétaire indirect de `receiver+0xdc` a déjà été observé avec la table
`0x8205A8EC` et les entrées `0x820D9A28`/`0x820D99F8`. Cette preuve qualifie
un contrat de receveur événementiel et une zone d'état générique; elle ne
qualifie pas un contrôleur de vol.

## `0x820DBF30` — construction du propriétaire

La décompilation headless de `0x820DBF30` montre :

- allocation d'un objet de `0x150` octets par le fournisseur de contexte,
  initialisé par `0x8237EDB0` (tail vers `0x823864E4`), puis stocké dans
  `context+0x04`;
- création conditionnelle d'un objet de `0x180` octets avec la vtable
  `PTR_Function_820E0298_8205A924`, champs d'état autour de `+0x5d/+0x5e`,
  puis appel de `0x8237E4B8`;
- deux allocations de `0x84` octets, initialisées par des méthodes vtable
  `+0x4c`, puis publiées à `context+0x1018` et `context+0x101c`;
- création finale d'un objet de `0x1c` octets avant l'initialisation de la
  structure de contexte.

Cela confirme un propriétaire de services/événements avec sous-objets, pas
une identité `PlayerAircraft`, `CameraController` ou `MissionController`.
Les appels directs vers `0x820DB500` et `0x820DBD90` ne peuvent pas être
qualifiés comme fonctions autonomes: les deux adresses sont intérieures à une
fonction/thunk selon `DecompileAt.java`.

## Décision de portée

La chaîne entrée numérique -> logique -> événement est suffisamment bornée
pour rester documentée et réutilisable par le code natif d'entrée. Elle ne
justifie toutefois pas :

- un nom de touche ou un mapping sémantique supplémentaire;
- un wrapper de commande de vol;
- une écriture d'état avion, caméra, arme ou mission;
- la promotion de `0x8237E4C0` en contrôleur de gameplay;
- une nouvelle session humaine.

La prochaine frontière AC6 est donc l'identité de l'objet qui consomme un
événement ou un slot logique et modifie effectivement l'avion actif, la
caméra de jeu ou une mission. Elle reste `needs-dynamic-evidence`. Ce statut
décrit une limite de preuve, non un blocage opérationnel de la tranche statique
actuelle.

## Validation exécutée

- Analyse headless en lecture seule avec Ghidra 12.1.2 sur
  `0x820DBF30`, `0x8237E4B0`, `0x8237E4C0`, `0x8237EDB0` et
  `0x820DB6D8`.
- Vérification que `0x820DB500` et `0x820DBD90` ne sont pas des débuts de
  fonction autonomes dans le programme courant.
- Relecture des rapports
  `FUNCTION_821CE088_PLAYER_INPUT_REPORT.md`,
  `FUNCTION_821BE268_DEFAULT_BINDINGS_REPORT.md`,
  `FUNCTION_821CE088_ANALOG_RECEIVER_REPORT.md`,
  `FUNCTION_820DB500_CONSUMER_EFFECTS_REPORT.md` et
  `FUNCTION_8237E4C0_EVENT_RECEIVER_REPORT.md`.
- CTest natif AC6: **41/41 PASS** en 2,04 s.
- Aucun appel cloud/local, Xenia, Wine, GUI ou intervention humaine.

## Suites

Conserver les noms offset-qualified et le statut
`needs-dynamic-evidence`. Réutiliser cette chaîne pour les tests de
normalisation et de diagnostic, mais ne pas l'utiliser pour déduire un
contrôle de vol tant qu'un consommateur final et sa mutation d'état n'ont pas
été observés. La campagne AC6 reste donc active dans le fil principal, sans
être oubliée ni artificiellement déclarée bloquée.
