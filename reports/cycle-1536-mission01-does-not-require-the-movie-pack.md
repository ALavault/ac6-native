# Cycle 1536 — Mission 01 ne requiert pas le pack vidéo

## Décision de scope

Le checkpoint 2 porte désormais sur le cône d'exécution de Mission 01. Dans
ce cône, aucune preuve statique ne relie `moviepack.bin` au briefing, au
gameplay ou au debrief. Le port du démultiplexage ASF et des vidéos du shell
n'est donc pas un prérequis de la preview M01. Le lecteur borné des 6 528
plages reste un garde de cache partagé ; il ne devient pas une sémantique
vidéo.

La même règle s'applique pour l'instant aux banques BGM et demo : elles ne
deviennent des exigences M01 que si une jointure statique ou une capture
oracle qualifiée les rend atteignables. Cette absence de jointure ne permet
pas de prétendre qu'elles ne seront jamais utilisées ; elle les sort seulement
du plus petit cône actuellement prouvé.

## Preuves PAL bornées

- cible `default.xex` PAL, SHA-256
  `acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde`,
  projet canonique `ghidra-projects/ace-combat-6` ;
- `0x821D7ED8` enregistre globalement `game:\moviepack.bin` au slot média 9,
  mais les exports canoniques de `CModeTaskTitleMovie` (`0x821B58F0`,
  `0x821B59C0`, `0x821B5A48`) ne portent ni identifiant M01, ni plage ASF, ni
  appel vers cette ouverture ;
- le briefing M01 est `DATA.TBL[210]` : `BRDB`, `BMAP`, `SWG` et huit RIFF
  internes. Aucun de ces nœuds ne rejoint `moviepack.bin` ;
- le gameplay est `DATA.TBL[9]`. Ses 44 groupes Scene/NFICCUT et ses 54 lignes
  `RadioTbl` ne publient aucun ordinal ASF ;
- le debrief natif courant ne possède encore aucun producteur média retail.

## Cône voix et texte restant

Le texte PAL est sélectionné par `0x821BAB70` puis `0x821BB118` : EN/fallback
utilise `DATA.TBL[4]`, DE `[5]`, IT `[6]`, FR `[7]` et ES `[8]`.
`0x821D2608` crée `TextData::[%d,%d]` et `0x821BB348` consomme les enfants
`root/0/0` et `root/0/1`; le second est `HASH`, tandis que le codec et la feuille
exacte des chaînes du premier restent ouverts.

`0x820A40D8` parcourt un flux signé 16 bits. Les valeurs inférieures à -1 sont
des marqueurs temporels, divisées par 10, mises à l'échelle par la constante
de `0x820547FC`, puis comparées au temps courant pour sélectionner une plage
visible. L'unité, le format complet et l'horloge audio ne sont pas encore
qualifiés.

La sélection voix est indépendante : `0x821249C8` choisit JP quand son second
paramètre vaut zéro et EN sinon. Le producteur qui relie la langue voix du
frontend à ce paramètre, puis la jointure
`RadioTbl -> TextData -> ordinal RIFF EN/JP -> horloge`, restent les frontières
de la lane média M01.

## Prochain lot vérifiable

1. corriger le mapping natif `FrontendLanguage` vers `{4,7,5,6,8}` dans
   l'ordre EN, FR, DE, IT, ES ;
2. borner la feuille et le codec `TextData`, y compris les marqueurs signés ;
3. joindre une ligne `RadioTbl` M01 à son texte, son ordinal RIFF et sa langue ;
4. décoder uniquement cette plage par AVIO bornée et tester la même horloge
   pour le cue audio et le sous-titre.

Aucune micro-exécution n'est nécessaire pour cette réduction de dépendances.
