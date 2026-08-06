# Cycle 1073 — borne statique owner/consumer objectifs-vagues

Date : 2026-08-06.

## Qualification

- Projet Ghidra canonique : `ghidra-projects/ace-combat-6`.
- Target : Xbox 360 PAL, `default.xex`.
- XEX SHA-256 : `acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde`.
- Module : `default.xex`; adresses dans l'image chargée Xenon.
- XenonRecomp est seulement un contrôle de flot littéral; aucun nom généré
  n'est promu comme sémantique.

## Méthode et résultat

Sans rescanner les 926 entrées de `DATA.TBL`, j'ai croisé les artefacts
statiques déjà qualifiés : le corpus ne qualifie aucun owner `SubMisTbl`,
`ComTbl` ou `Maneuver` (`0x8200F5A8` sans référence), les entrées briefing
210–224 ne publient aucun objectif/vague, et le census `0x822707C8` expose 230
objets sans registre d'unités, faction, cible ou transition d'objectif.

La frontière candidate reste `0x820A7F48`, avec constructeurs directs
`0x822A6560`, `0x822A8570` et `0x820A8E08`. Les trois acquisitions bornées ne
l'atteignent pas : aucun selector 4, record stable, champ variant ou insertion
`UnitManager`/`MissionManager` n'est observable. La paire owner/consumer exacte
reste donc ouverte; la borne négative est réduite à cette chaîne et au
consommateur générique `0x822707C8`.

Hypothèse historique intacte : le créateur/registre retail peut être derrière
`state40=8, selector44=4, type28=6 → type28=8`; son absence dans cette route ne
la réfute pas.

## Prochaine frontière précise

Après fermeture de cette transition, un hook read-only unique sur
`0x820A7F48` doit produire caller/LR, selector 4, arguments, adresse de record,
champ variant et avant/après du registre. Contrôle positif : insertion
atomique ou activation dans `UnitManager`/`MissionManager`. Limite : un vtable
ou un ordre de liste seul ne qualifie rien.
