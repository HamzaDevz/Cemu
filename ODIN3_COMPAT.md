# Compatibilité Odin 3 : Cemu (Wii U)

Notre propre liste de compatibilité, tenue d'après nos tests sur l'AYN Odin 3 (Snapdragon 8 Elite, Adreno 830) avec notre build de la branche `odin3`. Chaque test met à jour la ligne du jeu : statut, mesure, date et commit testé.

Autres émulateurs : [Eden (Switch)](https://github.com/HamzaDevz/eden/blob/odin3/ODIN3_COMPAT.md) · [Azahar (3DS)](https://github.com/HamzaDevz/azahar/blob/odin3/ODIN3_COMPAT.md) · [Dolphin (GC/Wii)](https://github.com/HamzaDevz/dolphin/blob/odin3/ODIN3_COMPAT.md) · [Cemu (Wii U)](https://github.com/HamzaDevz/Cemu/blob/odin3/ODIN3_COMPAT.md) · [Vita3K (Vita)](https://github.com/HamzaDevz/Vita3K/blob/odin3/ODIN3_COMPAT.md) · [aPS3e (PS3)](https://github.com/HamzaDevz/aps3e/blob/odin3/ODIN3_COMPAT.md) · [Bachata S4 (PS4)](https://github.com/HamzaDevz/bachata-s4/blob/odin3/ODIN3_COMPAT.md) · [xemu (Xbox)](https://github.com/HamzaDevz/xemu/blob/odin3/ODIN3_COMPAT.md)

## Échelle

| Statut | Sens |
|---|---|
| 🟢 Parfait | vu en partie, plein régime, aucun défaut vu |
| 🔵 Jouable | vu en partie, défauts mineurs |
| 🟡 Problèmes | vu en partie, défauts gênants (fps, graphismes, son) |
| 🟠 Menus/intro | démarre, pas encore vu en partie |
| 🔴 Ne marche pas | crash, écran noir ou blocage |
| ⚪ Non testé | présent sur la console, jamais lancé |

## Comment on teste

Paquet `info.cemu.cemu.odin3`, installé à côté de l'officiel `info.cemu.cemu` ; ES-DE le lance par défaut (« Cemu Odin 3 »). Banc adb dans `tools/odin3/` : fps relevés chaque seconde par SurfaceFlinger (`fps.sh`, `fps.py`), manettes DS4 virtuelles (`vpads.sh`), déplacements scriptés (`walk.sh`). Caches de shaders et de pipelines communautaires (emiyl) installés avant de mesurer. Config : `AsyncCompile`, `GX2DrawdoneSync` et `vkAccurateBarriers` actifs. Depuis le 22/10, banc odin3-lab (`emus/cemu`) : la même séquence scriptée sur l'Odin et dans une VM (lavapipe bridé au profil Vulkan de l'Adreno 830), images comparées au même instant de jeu.

## Jeux

| Jeu | Statut | Vu jusqu'à | Mesure | Notes | Testé le | Build |
|---|---|---|---|---|---|---|
| Super Mario 3D World (EUR) `0005000010145D00` | 🔵 Jouable | niveau 1-1 à 1, 2 et 3 joueurs | 60 images/s présentées du titre à l'intro d'une nouvelle partie (banc odin3-lab, 2 runs de 6000 images : 5939 et 5950 présentées sur 6023 et 6015, 21 et 0 manquées) ; 5,2 s de l'init de Cemu au code du jeu avec les caches ; le 17/10 : 60,0 fps, 0 % d'images hors cadence, pire image 16,7 ms (1 joueur), 60,1 fps à 3 joueurs | 4 joueurs impossibles quand le joueur 1 est en Pro Controller (3 au plus) ; sauvegardes à vérifier (3 fichiers « Nouveau » à chaque lancement) ; sortie TV pas testée ; correctifs 3f673d9c (manettes) et 4aa6d486 (60 Hz, plus de judder sur l'écran 120 Hz) ; démarrage ~30 s avec les caches (17/10) ; manettes façon Switch (d7ddd0ca, vu le 26/10 docké en 1-1) : un appui fait rejoindre une manette en J1 si l'Odin le tient, sinon au joueur libre suivant (DS4 en J1 avec son A qui valide l'écran titre, sonde en J2, l'Odin reprend J1 dès que les autres sont libres), ZL/ZR de l'Odin sur ses axes GAS/BRAKE (Luigi s'accroupit) ; le 26/10 un cache de pipelines corrompu (1379 entrées) a bloqué le chargement à 99 %, mis de côté ; changement de jeu depuis le frontend (SM3DW, Home, TPHD, Home, SM3DW) : chaque autre jeu démarre dans un processus neuf, le même jeu reprend là où il était (cf761840, 26/10) | 2026-10-26 | cf761840 |
| Zelda: Twilight Princess HD (EUR) `000500001019E600` | 🟠 Menus/intro | intro (85 s) | pas mesuré ; aucun shader recompilé grâce aux caches |  ; changement de jeu depuis le frontend (SM3DW, Home, TPHD, Home, SM3DW) : chaque autre jeu démarre dans un processus neuf, le même jeu reprend là où il était (cf761840, 26/10) | 2026-10-26 | cf761840 |
| Zelda: Breath of the Wild (USA v208 + DLC) `00050000101C9400` | 🟠 Menus/intro | écran titre | 30 fps au titre (plafond natif) ; premier lancement 3 min 17 s, puis 16 s | crashait au chargement du cache avant af6de619 ; ROM retirée de l'Odin le 17/10 (`Emulations/_retires/wiiu`), sauvegardes gardées | 2026-10-17 | af6de619 |
