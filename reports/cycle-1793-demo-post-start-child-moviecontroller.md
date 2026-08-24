# Cycle 1793 — MovieController enfant naturel après START

Date : 2026-08-23  
Cible : `ac6-demo-xbox360-pal`, module `Default.xex`  
Projet Ghidra canonique : `ghidra-projects/ace-combat-6-demo`  
Identité XEX requise :
`de917873f601e2a2208d75ab907e918ce941a42378d0d088705ecb4477405da8`

## Verdict

`CHILD_MOVIECONTROLLER_CONSTRUCTION_OBSERVED / ROOT_PUBLICATION_AND_FRONTEND_OPEN`

Le pulse START naturel au tick 3000, relâché au tick 3001, déclenche sous le
vrai `CModeTaskTitleDemoOffline` une invocation enfant de la factory virtuelle
`0x820D18C8`. Son adresse de retour est le callsite `0x8232356C`. La factory
appelle l'initialiseur `0x82323808` et retourne un `MovieController` cohérent
avec l'ABI statique qualifiée.

Cette observation réfute l'hypothèse d'absence de reconstruction post-START.
Elle ne prouve ni publication racine, ni remplacement de contrôleur, ni
`EndMode`, ni appel du listener Title, ni requête de transition au mode
manager. Le frontend, la mission et le terminal restent ouverts ;
`supported=false` reste inchangé.

## Jointure statique et runtime

La statique qualifie le caller naturel `0x82322300`, l'agrégat `A`, son
storage incorporé `B=A+0x08`, la sélection du descripteur `D` depuis les
tables de `B` et l'appel virtuel à `0x820D18C8`.

Au tick 3001, la trace observation-only relie :

```text
Title                = 0x2E3C0100, vtable 0x820113E4
factory              = 0x820D18C8
callsite retour      = 0x8232356C
receiver factory     = 0x2E3DF9D0, vtable 0x820064D8
A                     = 0x2E3E3AD4
B                     = 0x2E3E3ADC = A+0x08
D                     = 0x2DD7936C
s                     = -1
MovieController      = 0x2E3F8C50, vtable 0x820304D8
MovieMemory          = 0x2E3F8E90
ancien A+0xF4        = 0x2E3EDA90
```

Les champs relus dans l'objet retourné concordent avec `B`, `D`, `s` et
`MovieMemory`. Aucun shim, store guest, appel forcé de factory, remapping,
changement de scheduler ou pixel synthétique n'intervient.

## Limite causale

`0x8232356C` est un callsite, pas une fonction. L'observateur s'arrête au
retour de la factory enfant. Son verdict
`factory_result_without_root_publish` ne démontre donc pas l'absence d'un
consumer ou d'une publication ultérieure dans le corps appelant.

Le run se termine à `max_ticks=3407` avec :

```text
frontend = false
mission  = false
terminal = false
```

Le backend headless ne constitue pas une preuve de frame frontend visible.
Les entrées génériques ultérieures dans `0x82323808` ne sont pas corrélées à
cette invocation et ne sont pas utilisées pour le verdict.

## Prochain gate falsifiable

Qualifier statiquement le corps contenant le callsite `0x8232356C` :

1. identifier sa frontière de fonction et son owner ;
2. qualifier le prédicat qui déclenche la construction et la source de `D` ;
3. suivre le retour de `0x820D18C8` jusqu'à son consumer ou attribut de
   publication ;
4. seulement si cette chaîne le justifie, suivre un réarmement D5,
   `EndMode`/`menu_endMode`, le listener `0x8217C890` ou `manager+0x18`.

`done_when` : le consumer/publicateur est qualifié positivement, ou un
négatif borné exclut la publication dans le corps complet et désigne le
prochain producteur exact. Aucune nouvelle trace n'est autorisée avant qu'une
ambiguïté causale nommée survive à cette passe.

## Preuves autoritaires

- `artifacts/goal-playable/swg-acc-caller-join-static-20260823/RESULT.md`
- `artifacts/goal-playable/title-post-start-moviecontroller-join-runtime-20260823/RESULT.md`
- `analysis/demo/ac6-demo-structural-types-v1.json`

