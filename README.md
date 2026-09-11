# ChessLab v2.0 — Documentation technique

## Modifications de la v2

- Pack batterie **2S1P** (au lieu de 2×18650 en parallèle)
- Entrée **USB-C PD, négociée à 9V** (au lieu de 5V brut)
- Quatre LEDs par case au lieu d'une seule, pour améliorer le rendu
- Capteur à effet Hall simplifié
- ESP32-S3 pour plus de GPIO
- Gestion de la batterie intégrée sur PCB
- Gestion des voltages intégrée sur PCB
- PCB en 4 layers

Échiquier connecté détectant la position des pièces (capteurs à effet Hall) et affichant un retour visuel case par case (LEDs RGB adressables), pour visualiser une partie et aider à l'apprentissage du jeu.

## Architecture système

```
                        ESP32-S3
              WiFi · BLE · plus de GPIO
                            |
        +-------------------+-------------------+
        |                   |                   |
  Capteurs Hall         LEDs RGB           Alimentation
   (8x8 = 64)       (8x8x4 = 256)      Pack 2S1P + USB-C PD 9V
```

## Hardware

### Capteurs de position — SS49E

Capteur à effet Hall Analogique, suffisant pour détecter la présence ou l'absence d'une pièce sur une case.

### LEDs — WS2812B (NeoPixel)

4 LEDs par case (256 au total), câblées en une seule chaîne série (une ligne de données, montage serpentin case par case). Alimentation 5 V, consommation crête d'environ 60 mA par LED. Le courant crête théorique (toutes LEDs allumées en blanc plein) dépasse largement ce qu'un usage réel consommera (surbrillance ponctuelle de quelques cases) — **le dimensionnement du rail 5 V doit être validé sur la base de la consommation réelle en usage** (animation de coup, pas un blanc plein permanent).

### Multiplexage des capteurs Hall

64 capteurs analogiques dépassent le nombre de broches disponibles. Solution retenue : multiplexeurs analogiques 74HC4051 (8 voies vers 1), soit 8 multiplexeurs pour 64 capteurs.

### Alimentation — Pack 2S1P + USB-C PD

**Principe général : bus batterie unique**, permettant la charge et l'utilisation simultanées (comportement type UPS) sans logique de power-path complexe. Le chargeur et les régulateurs système sont câblés en parallèle sur les bornes du pack (B+/B-) : que l'USB soit branché ou non, le système tire son énergie du même nœud.

```
USB-C PD ──► Trigger PD fixe 9V (ex: CH224K) ──► TP5100 (charge 2S) ──┐
                                                                       │
                                                          Pack 2S1P (B+/B-), 6 à 8,4 V
                                                                       │
                                       ┌───────────────────────────────┼───────────────────────────────┐
                                       │                                                                │
                                Buck 5 V (LEDs WS2812B)                                  LDO / Buck 3,3 V (ESP32-S3 + capteurs)
```

**Pourquoi ce choix simplifie la v1 :**
- En 9V, l'entrée est déjà supérieure à la tension max du pack (8,4V) → un chargeur **linéaire** simple (TP5100) suffit, pas besoin d'étage boost comme le ferait une charge en 5V direct.
- Le TP5100 régule directement la tension totale du pack (mode 2S), sans nécessiter de fil d'équilibrage central — câblage à 2 fils (B+/B-) uniquement.
- Le bus batterie évite toute logique de priorisation charge/système : le système est simplement en parallèle de la charge, ça marche nativement en simultané.
- Les régulateurs de sortie passent de **boost** (v1 : MT3608, car VBAT ~3,7-4,2V < 5V) à **buck** (v2 : VBAT 6-8,4V > 5V), ce qui est généralement plus efficace et plus simple à stabiliser.

**Composants :**
- Trigger USB-C PD 9V : ex. CH224K (ou équivalent STUSB4500 si négociation plus fine nécessaire)
- Charge 2S1P : TP5100 (linéaire, alimenté en 9V, courant de charge ~1A programmable par résistance)
- Régulation 5V : convertisseur buck synchrone (ex. MP2315 ou équivalent), dimensionné selon la consommation LED réelle mesurée
- Régulation 3,3V : LDO AMS1117-3.3 (ou buck si la dissipation thermique devient un souci selon consommation ESP32-S3 + capteurs)

## Circuit d'alimentation (mise sous tension)

Commutation assurée par un MOSFET P-Channel AO3401, piloté par le bouton d'alimentation et maintenu par l'ESP32-S3 :

```
VBAT (pack 2S1P, 6-8,4V)
 |
 R1 10k
 |
 +----------------------> Gate AO3401 (P-MOS)
 |                              |
 |        +---- R2 100k --------+
 |        |
 |     Source AO3401 = VBAT
 |        |
 |       Drain --------------------> VSYS
 |                                     |
BTN_PWR                          (Buck 5V + LDO 3,3V)
 |                                     |
 +---- vers Gate                 ESP32-S3
                                   |         |
                          GPIO_PWR_HOLD  GPIO_BTN_PWR
                          (maintien ON)  (lecture bouton)
```

**Séquence d'allumage**
1. Appui sur BTN_PWR
2. La gate du MOSFET passe à GND, le MOSFET conduit
3. VSYS s'active, l'ESP32-S3 démarre
4. L'ESP32-S3 maintient son GPIO à l'état bas pour garder le MOSFET conducteur
5. Le bouton peut être relâché, le système reste alimenté

**Extinction logicielle**
1. Inactivité détectée (par exemple 10 minutes sans coup joué)
2. Sauvegarde de l'état de la partie si nécessaire
3. Le GPIO passe à l'état haut, le MOSFET se bloque et coupe l'alimentation

**Extinction manuelle**
1. Appui long sur BTN_PWR (environ 3 secondes)
2. Détection par l'ESP32-S3 via le GPIO de lecture du bouton
3. Même séquence que l'extinction logicielle

## Répartition des composants entre les deux cartes

| Carte principale | Carte d'alimentation |
|---|---|
| ESP32-S3 | Trigger USB-C PD (9V) |
| Connecteur USB-C | TP5100 (charge 2S1P) |
| 256× WS2812B (4/case) | Connecteur pack 2S1P (B+/B-) |
| 64× SS49E | Buck 5V (LEDs) |
| 8× 74HC4051 | LDO/Buck 3,3V |
| 1× 74HC138 | Commutation MOSFET |
| BTN_BOOT + BTN_RESET | BTN_PWR + LED power |
| Connecteur JST vers carte d'alimentation | Connecteur JST vers carte principale |

Les deux cartes sont reliées par un connecteur JST 4 broches :

| Broche | Signal |
|---|---|
| 1 | VBAT (pack 2S1P, 6-8,4V) |
| 2 | GND |
| 3 | 5 V (sortie buck vers carte principale) |
| 4 | 3,3 V (sortie LDO/buck vers carte principale) |

## Layout PCB

- Routage serpentin des WS2812B (entrée/sortie de données case par case)
- Matrices de capteurs Hall organisées en lignes/colonnes vers les multiplexeurs
- PCB en 4 layers (plans dédiés alimentation/masse pour limiter les chutes de tension liées à l'augmentation du nombre de LEDs)
- Couche diffusante imprimée en 3D au-dessus des LEDs

### Largeurs de piste

| Signal | Largeur |
|---|---|
| Données / GPIO | 0,25 mm |
| Alimentation 3,3 V | 0,5 mm |
| Alimentation 5 V (LEDs) | à revalider selon consommation réelle (256 LEDs vs 64 en v1) |
| VBAT | 1,0 mm (voire plus, courant de charge + tirage système cumulés) |
| VSYS | 1,0 mm |
| USB D+ / D- | 0,2 mm |

### Vias

| Type | Perçage | Pad |
|---|---|---|
| Standard | 0,4 mm | 0,8 mm |
| Puissance | 0,6 mm | 1,2 mm |

## Pinout ESP32-S3

*À redéfinir en détail (migration C6 → S3, plus de GPIO disponibles). Les fonctions liées à l'alimentation restent inchangées dans leur principe :*

| Fonction | Notes |
|---|---|
| Lecture bouton d'alimentation (BTN_PWR) | GPIO à définir |
| Maintien alimentation (PWR_HOLD) | GPIO à définir |
| Suivi tension batterie (ADC) | Diviseur de tension à recalculer pour la plage 2S1P (6-8,4V au lieu de ~3-4,2V en v1) |

## Fixation des pièces

Aimants cylindriques N52, Ø6×3 mm, insérés dans la base de chaque pièce et collés à l'époxy dans un logement imprimé en 3D.

## Boîtier

Hauteur totale estimée : 25 à 30 mm, dimensionnée pour loger la carte principale, la carte d'alimentation et le pack 2S1P.

## Firmware

Architecture logicielle sous FreeRTOS, organisée en tâches indépendantes :

| Tâche | Rôle |
|---|---|
| Hall Scanner | Scrutation des 64 capteurs (polling 50 ms) |
| LED Controller | Traduction de l'état de l'échiquier en animation LED |
| Chess Engine | Validation des positions et des coups légaux |
| BLE/WiFi | Communication avec l'application mobile ou l'interface web |
| Battery Monitor | Suivi de la tension batterie via diviseur de tension sur ADC (plage recalculée pour 2S1P) |

**Bibliothèques envisagées**
- `FastLED` ou `Adafruit NeoPixel` pour le pilotage des WS2812B
- `Stockfish` (version allégée, via WASM) pour le moteur d'échecs
- Interface web servie depuis la mémoire flash (SPIFFS/LittleFS)

## Feuille de route

| Phase | Objectif |
|---|---|
| P1 | Prototype d'une case (4 LEDs + 1 capteur Hall + multiplexeur) sur breadboard |
| P2 | PCB de test 4×4 (KiCad) pour valider le multiplexage |
| P3 | PCB complet 8×8 et boîtier v2 |
| P4 | Firmware — moteur d'échecs et application |
| P5 | Impression 3D du boîtier final et des pièces modifiées |
