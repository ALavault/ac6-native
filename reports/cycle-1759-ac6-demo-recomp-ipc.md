# AC6 `demo-recomp` IPC — checkpoint cycle 1759

Le transport `demo-recomp` est présent comme surface IPC bornée, mais le
backend reste `supported=false` pour la qualification PAL. L’identité attendue
est `ac6-demo-xbox360-pal`, `Default.xex`, SHA-256
`de917873f601e2a2208d75ab907e918ce941a42378d0d088705ecb4477405da8`.

Architecture : socketpair privé AF_UNIX/SOCK_STREAM, `Popen(shell=False)` avec
`close_fds=True`, `pass_fds` limité au fd enfant et argv fixe
`--emu-agent-ipc-fd <fd>`. Le framing est une longueur big-endian de 4 octets
suivie d’un objet JSON canonique borné à 64 KiB. Le token est aléatoire et les
deadlines sont bornées à 30 s par défaut / 120 s maximum. Le processus et le
socket sont possédés par une session et fermés ensemble. Le binaire C++ entre
par fd dans `run_emu_agent_ipc` et possède un `DemoSession`.

Les états sont distingués : `initial_unavailable` couvre l’absence du binaire
ou du backend au boot; `transport_failed` empoisonne une session après une
déconnexion/erreur. Le replay démarre une session distincte depuis un reçu
immuable et annule la nouvelle session/son transport en cas d’échec.

Les réponses demo-recomp restent `availability=unavailable` et
`qualified=false`; l’allowlist SHA-256 PAL est vide. Le transport MCP stdio
est distinct du transport émulateur. Aucun run PAL, succès mission, claim
frontend ou preuve Xenia n’est promu.

Validations :

```text
pytest MCP v2 + FSM + transport : 54 passed, 1 skipped
py_compile : PASS
codegen OFF : CTest 19/19
codegen ON : CTest 18/18
deux codegens : manifests égaux, generated/55 fichiers égaux, objects/53 fichiers égaux
installation canonique démo vers racine portfolio : PASS; test ! -e bin/bin
```

Les chemins, hashes sources, manifest/objet codegen et hashes du reçu sont
dans le JSON associé.
