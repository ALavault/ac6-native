# Contre-vérification de la porte de soumission, sur notre propre image

Date : 2026-08-18
Objet : `reports/AC6_DEMO_PAL_RENDER_EVENT_GATE_CLOSURE.md`, importé de
`origin/infos` (`aa3a629f`)

## Ce qui converge

Ce rapport et les mesures dynamiques de `main` se sont rejoints sans se
connaître, sur les deux mêmes faits :

| fait | mesuré ici | rapport importé |
|---|---|---|
| porte de soumission | `[device+21600]`, nul sur 5 600 ticks | `device+0x5460` |
| unique écrivain | `sub_821ADAB8`, jamais atteinte | `demo:0x821ADAB8` |

`21600 = 0x5460`. Les deux voies — l'une par trace runtime, l'autre par
analyse statique — désignent le même champ et le même armeur.

## Ce que le rapport ajoute, vérifié ici

Son outil `tools/verify_ac6_pal_renderer_xaudio.py` exige une image mémoire
de SHA-256 `b8194199…`, que ce dépôt n'a pas comme fichier ; notre basefile
est `b98a9ac1…`. Sa vérification n'est donc **pas rejouable ici**, et ses
affirmations ont été contrôlées à la main sur `.build/Default.xex.base.bin` :

```text
0x821ADB14  0x2B030011  cmplwi r3,17      ; événement 17
0x821ADB24  0x2B040006  cmplwi r4,6       ; canal 6
0x821ADB40  0x913F5460  stw r9,0x5460(r31); arme la soumission
0x821ADB44  0x917F5458                     ; index de file remis à zéro
0x821ADB48  0x917F545C
```

et le SHA-256 des 448 octets `0x821ADAB8..0x821ADC77` vaut
`723dcd6f1680e5bfa22657510176790ee9fec54c301b3d651a07a468ca4fdc85`,
identique à celui qu'il annonce. **Tout tient.**

## Ce que cela déplace

L'enregistrement du callback n'est pas le problème : `sub_821ADC78` **est
atteinte** par la route par défaut — elle figure dans l'atlas de réachabilité
de la sonde. Elle construit un descripteur `{type=2, callback=0x821ADAB8}` et
le passe avec `r3=47`.

Mais ce « service 47 » est un `bctrl` sur une cible chargée depuis un global :
c'est le **registre de services du jeu**, pas un import noyau. Ni
l'enregistrement ni la livraison ne passent donc par une frontière que ce
runtime pourrait servir.

La frontière restante est donc : **quel code guest lève l'événement (17, 6)**,
et pourquoi il n'est pas atteint. Ce n'est pas une valeur à synthétiser côté
pont — la synthétiser fabriquerait un comportement guest, ce que le produit
refuse.
