# Cycle 1795 — promotion et premier tick du `MovieController` enfant

## Cible qualifiée

- projet Ghidra canonique : `ghidra-projects/ace-combat-6-demo`
- module : `Default.xex` de la démo PAL
- fonction : `0x82323BB8`, update du `swg::MovieController`
- producteur enfant joint : `0x82323468..0x823235CF`

## Verdict

`PENDING_PROMOTION_AND_FIRST_TICK_QUALIFIED`

La boucle `0x82323BB8` contient elle-même la promotion recherchée. Au début
de chaque itération stable de frame, pour le contrôleur courant `A` :

```text
old_pending = A+0xE4
A+0xE4 = 0
A+0xE8 = 0
A+0xEC = 0
A+0xF4 = old_pending
A+0xF0 = 0
```

Elle traite ensuite les éléments de la frame active. Un handler peut alors
appeler `0x82323468`, qui construit ou réutilise un enfant et l'insère dans la
nouvelle liste `A+0xE4/A+0xE8`. La liste active `A+0xF4`, capturée avant ces
handlers, est parcourue par les liens enfant `+0x18` et enregistrée auprès du
service virtuel du `MovieMemory`.

Après stabilisation de la frame, `0x82323BB8` parcourt la nouvelle liste
`A+0xE4` par les mêmes liens `+0x18` et s'appelle récursivement sur chaque
enfant avec la transformation calculée du parent :

```text
for (child = A+0xE4; child != 0; child = child+0x18)
    0x82323BB8(child, parent_transform)
```

L'enfant créé pendant le tick reçoit donc son premier update dans cette même
invocation parentale. Au début du tick parent suivant, la tête pending est
publiée dans `A+0xF4`. Dans l'appel récursif, les offsets `E4/F4` appartiennent
à l'enfant lui-même ; ils ne doivent pas être confondus avec ceux du parent.

## Conséquence

L'absence de publication `A+0xF4` dans le corps constructeur n'était pas un
blocage : la structure est un double temps pending/active piloté par l'update.
La route post-START atteint donc naturellement la mise à jour du nouveau
contrôleur de timeline UI scriptée. Cela ne démontre toujours ni `EndMode`, ni
listener Title, ni écriture `manager+0x18`, ni frame frontend post-transition.

Le prochain gate doit suivre, depuis ce premier tick enfant uniquement, le
premier effet persistant qui quitte la timeline SWG : commande de VM, callback
natif ou autre propriétaire qualifié. Une trace n'est autorisée que si la
statique laisse un prédicat causal nommé irréductible.

`frontend=false`, `mission=false`, `terminal=false`, `supported=false`.

