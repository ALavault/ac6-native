# Cycle 339 — le constructeur du dialogue est localisé ; l'identifiant de chaîne ne l'est pas

## 1. Localisation, mesurée

Sonde posée sur `XamShowDeviceSelectorUI`, journalisant l'appelant invité
(`lr - 4`, même méthode que `CurrentGuestCallerAddress`) :

```
[ac6-dialog] XamShowDeviceSelectorUI called from guest 0x821CE928
```

Le site d'appel `0x821CE928` est dans **`sub_821CE8E0`**
(`generated/ac6recomp_recomp.14.cpp`, 75 lignes). Le code généré au site :

```c
	// li r5,512
	// lwz r3,36(r31)
	// li r4,1
	// bl 0x821f4658
	ctx.lr = 0x821CE92C;
	sub_821F4658(ctx, base);
	// cmplwi cr6,r3,997
	ctx.cr6.compare<uint32_t>(ctx.r3.u32, 997, ctx.xer);
```

Chaîne établie : **`sub_821CE8E0` -> `sub_821F4658` -> `XamShowDeviceSelectorUI`**.

Le retour est comparé à **997**, et la fonction voisine documentée est
`FUNCTION_821CE088` (entrée joueur / récepteur analogique) : ce module est bien
celui de l'interface, ce qui concorde.

## 2. Ce qui n'est pas fait, et pourquoi

**L'identifiant de chaîne n'est pas journalisé.** La sonde a atteint le chemin
du *sélecteur de périphérique*, pas celui du *texte*. `sub_821CE8E0` prépare un
tampon (`r31+92`, 7 mots) et un appel de stockage ; rien dans ces 75 lignes ne
ressemble à une recherche de chaîne.

Poser le crochet demandé exige d'abord d'identifier la fonction qui **dessine**
le dialogue — voisine, pas identique. Le point de départ est acquis : le module
`0x821CE***`, et `sub_821F4658` dont le code de retour 997 pilote la branche.

**Les exécutions oracle avec l'émulateur ne sont pas préparées.** Rien n'a été
lancé, rien n'est configuré au-delà de ce qui existait. À noter pour la reprise :
le cycle 316 avait mesuré que Xenia sous Xvfb **ne rend rien** — captures
identiques à l'octet près. Une comparaison visuelle exige donc un affichage
headless **accéléré par GPU** (Xorg + pilote NVIDIA, écran virtuel), pas Xvfb,
et jamais la session de bureau de l'opérateur. Le profil Xenia existant sous
`.tools/xenia-canary-windows/.../E030000042B27D70/` est à **préserver** :
ni commit, ni écrasement, ni renommage.

## 3. Front suivant

1. Trouver la fonction voisine qui construit l'affichage du dialogue et y poser
   un `[[midasm_hook]]` journalisant l'identifiant de chaîne demandé et le
   pointeur rendu.
2. Monter l'affichage headless GPU, puis capturer la même séquence sous Xenia
   pour comparaison visuelle — le dialogue y a-t-il son texte ?
3. Le cycle 338 a montré qu'aucune lecture invitée n'échoue : la chaîne est
   présente sur disque. Le défaut est donc dans la sélection ou l'assignation,
   pas dans l'accès aux données.

## 4. État global inchangé

P1.3 non franchie. P2 à P7 non faits. La première mission ne se joue pas.
`recompiler-generated` n'est pas `verified`.
