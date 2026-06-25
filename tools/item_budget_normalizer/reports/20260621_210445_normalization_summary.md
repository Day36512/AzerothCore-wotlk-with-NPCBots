# Item Budget Normalizer Summary

- Generated: 2026-06-21T21:04:45
- Input CSV: `F:\Downloads\First_pass_normalization.csv`
- Config: `C:\AzerothCore\Burning_ Legacy_Server\AzerothCore-wotlk-with-NPCBots\tools\item_budget_normalizer\item_budget_config.yml`
- SQL generation: disabled (report-only dry run)
- Filters: min ilvl `110`, max ilvl `164`, required level `70`, qualities `any`

## Output Files

- Before/after CSV: `tools\item_budget_normalizer\reports\20260621_210445_normalization_report.csv`
- Calibration CSV: `tools\item_budget_normalizer\reports\20260621_210445_calibration.csv`
- Update SQL: not generated
- Rollback SQL: not generated

## Counts

- Report rows: 3108
- Skipped: 125
- Report-only changes: 2981
- Updated SQL rows: 0
- Unchanged: 2
- Would change if SQL were enabled: 2981

## Skipped Items And Why

| Reason | Count |
| --- | --- |
| cannot classify inventory slot | 116 |
| trinket excluded | 108 |
| not armor or weapon class | 9 |
| name matched exclusion pattern Test  | 4 |
| duplicate stat type 45 in slot 6 | 2 |
| duplicate stat type 31 in slot 6 | 1 |
| name matched exclusion pattern Monster - | 1 |
| non-contiguous stat slot 3 | 1 |
| non-contiguous stat slot 4 | 1 |
| non-contiguous stat slot 5 | 1 |
| non-contiguous stat slot 6 | 1 |

## Most Overbudget Before Normalization

| entry | name | item_level | quality_label | slot_key | percent_over_under_before | action |
| --- | --- | --- | --- | --- | --- | --- |
| 34198 | Stanchion of Primal Instinct | 154 | Epic | weapon_two_hand | 462.843 | report-only |
| 35103 | Brutal Gladiator's Staff | 154 | Epic | weapon_two_hand | 461.247 | report-only |
| 33716 | Vengeful Gladiator's Staff | 146 | Epic | weapon_two_hand | 453.451 | report-only |
| 33987 | Indalamar's Ring of 200 Spell Crit | 130 | Epic | finger | 446.84 | report-only |
| 32014 | Merciless Gladiator's Maul | 136 | Epic | weapon_two_hand | 446.642 | report-only |
| 28476 | Gladiator's Maul | 123 | Epic | weapon_two_hand | 438.633 | report-only |
| 34898 | Staff of the Forest Lord | 146 | Epic | weapon_two_hand | 427.935 | report-only |
| 28919 | High Warlord's Maul | 115 | Rare | weapon_two_hand | 340.0 | report-only |
| 28948 | Grand Marshal's Maul | 115 | Rare | weapon_two_hand | 340.0 | report-only |
| 27877 | Draenic Wildstaff | 115 | Rare | weapon_two_hand | 327.576 | report-only |
| 34335 | Hammer of Sanctification | 164 | Epic | weapon_main_hand | 308.154 | report-only |
| 28325 | Dreamer's Dragonstaff | 115 | Rare | weapon_two_hand | 306.907 | report-only |
| 30910 | Tempest of Chaos | 151 | Epic | weapon_main_hand | 294.206 | report-only |
| 34336 | Sunflare | 164 | Epic | weapon_main_hand | 293.267 | report-only |
| 33354 | Wub's Cursed Hexblade | 132 | Epic | weapon_main_hand | 292.103 | report-only |
| 34199 | Archon's Gavel | 154 | Epic | weapon_main_hand | 291.059 | report-only |
| 34547 | Onslaught Waistguard | 154 | Epic | waist | 289.404 | report-only |
| 35014 | Brutal Gladiator's Gavel | 154 | Epic | weapon_main_hand | 289.206 | report-only |
| 35102 | Brutal Gladiator's Spellblade | 154 | Epic | weapon_main_hand | 289.206 | report-only |
| 30108 | Lightfathom Scepter | 141 | Epic | weapon_main_hand | 285.915 | report-only |

## Most Underbudget Before Normalization

| entry | name | item_level | quality_label | slot_key | percent_over_under_before | action |
| --- | --- | --- | --- | --- | --- | --- |
| 28438 | Dragonmaw | 123 | Epic | weapon_one_hand | -90.16 | report-only |
| 32336 | Black Bow of the Betrayer | 151 | Epic | ranged | -83.244 | report-only |
| 28439 | Dragonstrike | 136 | Epic | weapon_one_hand | -81.494 | report-only |
| 32262 | Syphon of the Nathrezim | 141 | Epic | weapon_one_hand | -65.135 | report-only |
| 29996 | Rod of the Sun King | 141 | Epic | weapon_one_hand | -63.74 | report-only |
| 29269 | Sapphiron's Wing Bone | 110 | Epic | held_in_off_hand | -60.24 | report-only |
| 29272 | Orb of the Soul-Eater | 110 | Epic | held_in_off_hand | -60.041 | report-only |
| 32471 | Shard of Azzinoth | 151 | Epic | weapon_one_hand | -58.754 | report-only |
| 28767 | The Decapitator | 125 | Epic | weapon_one_hand | -56.752 | report-only |
| 24092 | Pendant of Frozen Flame | 114 | Rare | neck | -55.897 | report-only |
| 24093 | Pendant of Thawing | 114 | Rare | neck | -55.897 | report-only |
| 24095 | Pendant of Withering | 114 | Rare | neck | -55.897 | report-only |
| 24097 | Pendant of Shadow's End | 114 | Rare | neck | -55.897 | report-only |
| 24098 | Pendant of the Null Rune | 114 | Rare | neck | -55.897 | report-only |
| 29271 | Talisman of Kalecgos | 110 | Epic | held_in_off_hand | -53.614 | report-only |
| 32422 | Tom's Axe B | 130 | Epic | weapon_two_hand | -53.215 | report-only |
| 32082 | The Fel Barrier | 110 | Epic | shield | -49.602 | report-only |
| 28367 | Greatsword of Forlorn Visions | 115 | Rare | weapon_two_hand | -49.584 | report-only |
| 30666 | Ritssyn's Lost Pendant | 115 | Epic | neck | -49.377 | report-only |
| 27507 | Adamantine Repeater | 115 | Rare | ranged | -49.314 | report-only |

## Weapons Changed

| entry | name | item_level | quality_label | subclass_label | old_dps | new_dps | action | warnings |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 29204 | Felsteel Whisper Knives | 110 | Rare | Thrown | 61.136 | 62.5 | report-only | weapon DPS median from 5 sample(s) within 10 ilvl |
| 29275 | Searing Sunblade | 110 | Epic | Dagger | 85.385 | 85.385 | report-only | weapon damage unchanged: not enough comparable physical weapon samples |
| 29346 | Feltooth Eviscerator | 110 | Epic | Dagger | 85.357 | 83.214 | report-only | weapon DPS median from 8 sample(s) within 10 ilvl |
| 29350 | The Black Stalk | 110 | Epic | Wand | 157.333 | 157.333 | report-only | weapon damage unchanged: not enough comparable caster weapon samples |
| 35514 | Frostscythe of Lord Ahune | 110 | Epic | Staff | 67.094 | 67.5 | report-only | spell payload ignored and left unchanged; weapon DPS median from 4 sample(s) within 10 ilvl |
| 23748 | Ornate Khorium Rifle | 115 | Rare | Gun | 66.452 | 64.677 | report-only | weapon DPS median from 7 sample(s) within 10 ilvl |
| 27463 | Terror Flame Dagger | 115 | Rare | Dagger | 71.667 | 71.667 | report-only | weapon DPS median from 16 sample(s) within 10 ilvl |
| 27476 | Truncheon of Five Hells | 115 | Rare | Mace | 71.667 | 71.667 | report-only | weapon DPS median from 7 sample(s) within 10 ilvl |
| 27490 | Firebrand Battleaxe | 115 | Rare | Axe | 71.667 | 71.667 | report-only | weapon DPS median from 9 sample(s) within 10 ilvl |
| 27507 | Adamantine Repeater | 115 | Rare | Crossbow | 66.333 | 66.333 | report-only | weapon damage unchanged: not enough comparable physical weapon samples |
| 27512 | The Willbreaker | 115 | Rare | Sword | 41.367 | 41.111 | report-only | weapon DPS median from 5 sample(s) within 10 ilvl |
| 27533 | Demonblood Eviscerator | 115 | Rare | Fist | 71.731 | 71.731 | report-only | weapon DPS median from 4 sample(s) within 10 ilvl |
| 27538 | Lightsworn Hammer | 115 | Rare | Mace | 41.367 | 41.111 | report-only | weapon DPS median from 8 sample(s) within 10 ilvl |
| 27540 | Nexus Torch | 115 | Rare | Wand | 129.444 | 125.278 | report-only | weapon DPS median from 5 sample(s) within 10 ilvl |
| 27543 | Starlight Dagger | 115 | Rare | Dagger | 41.367 | 41.0 | report-only | weapon DPS median from 5 sample(s) within 10 ilvl |
| 27673 | Phosphorescent Blade | 115 | Rare | Sword | 71.667 | 71.667 | report-only | weapon DPS median from 9 sample(s) within 10 ilvl |
| 27741 | Bleeding Hollow Warhammer | 115 | Rare | Mace | 41.367 | 41.25 | report-only | weapon DPS median from 8 sample(s) within 10 ilvl |
| 27747 | Boggspine Knuckles | 115 | Rare | Fist | 71.731 | 71.731 | report-only | weapon DPS median from 4 sample(s) within 20 ilvl |
| 27757 | Greatstaff of the Leviathan | 115 | Rare | Staff | 63.033 | 61.667 | report-only | weapon DPS median from 5 sample(s) within 10 ilvl |
| 27767 | Bogreaver | 115 | Rare | Axe | 71.765 | 71.765 | report-only | weapon DPS median from 9 sample(s) within 10 ilvl |
| 27769 | Endbringer | 115 | Rare | Two-hand Sword | 93.235 | 93.235 | report-only | weapon DPS median from 4 sample(s) within 10 ilvl |
| 27814 | Twinblade of Mastery | 115 | Rare | Dagger | 71.786 | 71.786 | report-only | weapon DPS median from 16 sample(s) within 10 ilvl |
| 27817 | Starbolt Longbow | 115 | Rare | Bow | 66.429 | 66.429 | report-only | weapon DPS median from 8 sample(s) within 10 ilvl |
| 27829 | Axe of the Nexus-Kings | 115 | Rare | Two-hand Axe | 93.235 | 93.235 | report-only | weapon DPS median from 11 sample(s) within 10 ilvl |
| 27840 | Scepter of Sha'tar | 115 | Rare | Two-hand Mace | 93.286 | 93.286 | report-only | weapon DPS median from 8 sample(s) within 10 ilvl |
| 27842 | Grand Scepter of the Nexus-Kings | 115 | Rare | Staff | 63.033 | 62.917 | report-only | weapon DPS median from 17 sample(s) within 10 ilvl |
| 27846 | Claw of the Watcher | 115 | Rare | Fist | 71.6 | 71.6 | report-only | socket budget counted; socket colors and socket bonus unchanged; weapon DPS median from 4 sample(s) within 10 ilvl |
| 27872 | The Harvester of Souls | 115 | Rare | Axe | 71.731 | 71.731 | report-only | weapon DPS median from 9 sample(s) within 10 ilvl |
| 27876 | Will of the Fallen Exarch | 115 | Rare | Mace | 41.367 | 41.111 | report-only | weapon DPS median from 8 sample(s) within 10 ilvl |
| 27877 | Draenic Wildstaff | 115 | Rare | Staff | 63.033 | 61.667 | report-only | weapon DPS median from 5 sample(s) within 10 ilvl |

## Armor Changed

| entry | name | item_level | quality_label | subclass_label | old_armor | new_armor | action | warnings |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 29237 | Warpath Bracers | 110 | Epic | Plate | 608 | 624 | report-only | armor median from 15 sample(s) within 10 ilvl |
| 29238 | Lion's Heart Girdle | 110 | Epic | Plate | 782 | 870 | report-only | armor median from 22 sample(s) within 20 ilvl |
| 29239 | Eaglecrest Warboots | 110 | Epic | Plate | 955 | 997 | report-only | armor median from 8 sample(s) within 10 ilvl |
| 29240 | Bands of Negation | 110 | Epic | Cloth | 81 | 84 | report-only | armor median from 13 sample(s) within 10 ilvl |
| 29242 | Boots of Blasphemy | 110 | Epic | Cloth | 128 | 134 | report-only | armor median from 8 sample(s) within 10 ilvl |
| 29243 | Wave-Fury Vambraces | 110 | Epic | Mail | 340 | 349 | report-only | armor median from 13 sample(s) within 10 ilvl |
| 29244 | Wave-Song Girdle | 110 | Epic | Mail | 438 | 457 | report-only | armor median from 6 sample(s) within 10 ilvl |
| 29245 | Wave-Crest Striders | 110 | Epic | Mail | 535 | 558 | report-only | armor median from 5 sample(s) within 10 ilvl |
| 29246 | Nightfall Wristguards | 110 | Epic | Leather | 153 | 173 | report-only | armor median from 14 sample(s) within 10 ilvl |
| 29247 | Girdle of the Deathdealer | 110 | Epic | Leather | 196 | 205 | report-only | armor median from 5 sample(s) within 10 ilvl |
| 29248 | Shadowstep Striders | 110 | Epic | Leather | 240 | 250 | report-only | armor median from 8 sample(s) within 10 ilvl |
| 29249 | Bands of the Benevolent | 110 | Epic | Cloth | 81 | 84 | report-only | armor median from 13 sample(s) within 10 ilvl |
| 29251 | Boots of the Pious | 110 | Epic | Cloth | 128 | 134 | report-only | armor median from 8 sample(s) within 10 ilvl |
| 29252 | Bracers of Dignity | 110 | Epic | Plate | 608 | 624 | report-only | armor median from 15 sample(s) within 10 ilvl |
| 29253 | Girdle of Valorous Deeds | 110 | Epic | Plate | 782 | 870 | report-only | armor median from 22 sample(s) within 20 ilvl |
| 29254 | Boots of the Righteous Path | 110 | Epic | Plate | 955 | 997 | report-only | armor median from 8 sample(s) within 10 ilvl |
| 29255 | Bands of Rarefied Magic | 110 | Epic | Cloth | 81 | 84 | report-only | armor median from 13 sample(s) within 10 ilvl |
| 29258 | Boots of Ethereal Manipulation | 110 | Epic | Cloth | 128 | 134 | report-only | armor median from 8 sample(s) within 10 ilvl |
| 29259 | Bracers of the Hunt | 110 | Epic | Mail | 340 | 349 | report-only | armor median from 13 sample(s) within 10 ilvl |
| 29261 | Girdle of Ferocity | 110 | Epic | Mail | 438 | 457 | report-only | armor median from 6 sample(s) within 10 ilvl |
| 29262 | Boots of the Endless Hunt | 110 | Epic | Mail | 535 | 558 | report-only | armor median from 5 sample(s) within 10 ilvl |
| 29263 | Forestheart Bracers | 110 | Epic | Leather | 237 | 159 | report-only | armor median from 14 sample(s) within 10 ilvl |
| 29264 | Tree-Mender's Belt | 110 | Epic | Leather | 406 | 205 | report-only | armor median from 5 sample(s) within 10 ilvl |
| 29265 | Barkchip Boots | 110 | Epic | Leather | 352 | 250 | report-only | armor median from 8 sample(s) within 10 ilvl |
| 29354 | Light-Touched Stole of Altruism | 110 | Epic | Cloth | 93 | 97 | report-only | armor median from 25 sample(s) within 10 ilvl |
| 29357 | Master Thief's Gloves | 110 | Epic | Leather | 218 | 228 | report-only | spell payload ignored and left unchanged; armor median from 12 sample(s) within 10 ilvl |
| 29369 | Shawl of Shifting Probabilities | 110 | Epic | Cloth | 93 | 97 | report-only | armor median from 25 sample(s) within 10 ilvl |
| 29375 | Bishop's Cloak | 110 | Epic | Cloth | 93 | 97 | report-only | armor median from 25 sample(s) within 10 ilvl |
| 29382 | Blood Knight War Cloak | 110 | Epic | Cloth | 93 | 97 | report-only | armor median from 25 sample(s) within 10 ilvl |
| 29384 | Ring of Unyielding Force | 110 | Epic | Misc | 294 | 287 | report-only | armor median from 4 sample(s) within 10 ilvl |

## Items With Sockets

| entry | name | item_level | quality_label | slot_key | socket_budget | action | warnings |
| --- | --- | --- | --- | --- | --- | --- | --- |
| 30531 | Breeches of the Occultist | 110 | Epic | legs | 24.0 | report-only | socket budget counted; socket colors and socket bonus unchanged; armor median from 16 sample(s) within 10 ilvl |
| 30532 | Kirin Tor Master's Trousers | 110 | Epic | legs | 24.0 | report-only | socket budget counted; socket colors and socket bonus unchanged; armor median from 16 sample(s) within 10 ilvl |
| 30533 | Vanquisher's Legplates | 110 | Epic | legs | 24.0 | report-only | socket budget counted; socket colors and socket bonus unchanged; armor median from 12 sample(s) within 10 ilvl |
| 30534 | Wyrmscale Greaves | 110 | Epic | legs | 24.0 | report-only | socket budget counted; socket colors and socket bonus unchanged; armor median from 11 sample(s) within 10 ilvl |
| 30535 | Forestwalker Kilt | 110 | Epic | legs | 24.0 | report-only | socket budget counted; socket colors and socket bonus unchanged; armor median from 10 sample(s) within 10 ilvl |
| 30536 | Greaves of the Martyr | 110 | Epic | legs | 24.0 | report-only | socket budget counted; socket colors and socket bonus unchanged; armor median from 12 sample(s) within 10 ilvl |
| 30538 | Midnight Legguards | 110 | Epic | legs | 24.0 | report-only | socket budget counted; socket colors and socket bonus unchanged; armor median from 10 sample(s) within 10 ilvl |
| 30541 | Stormsong Kilt | 110 | Epic | legs | 24.0 | report-only | socket budget counted; socket colors and socket bonus unchanged; armor median from 11 sample(s) within 10 ilvl |
| 30543 | Pontifex Kilt | 110 | Epic | legs | 24.0 | report-only | socket budget counted; socket colors and socket bonus unchanged; armor median from 16 sample(s) within 10 ilvl |
| 32083 | Faceguard of Determination | 110 | Epic | head | 20.0 | report-only | socket budget counted; socket colors and socket bonus unchanged; armor median from 13 sample(s) within 10 ilvl |
| 32084 | Helmet of the Steadfast Champion | 110 | Epic | head | 20.0 | report-only | socket budget counted; socket colors and socket bonus unchanged; armor median from 13 sample(s) within 10 ilvl |
| 32085 | Warpstalker Helm | 110 | Epic | head | 20.0 | report-only | socket budget counted; socket colors and socket bonus unchanged; armor median from 12 sample(s) within 10 ilvl |
| 32086 | Storm Master's Helmet | 110 | Epic | head | 20.0 | report-only | socket budget counted; socket colors and socket bonus unchanged; armor median from 12 sample(s) within 10 ilvl |
| 32087 | Mask of the Deceiver | 110 | Epic | head | 20.0 | report-only | socket budget counted; socket colors and socket bonus unchanged; armor median from 10 sample(s) within 10 ilvl |
| 32088 | Cowl of Beastly Rage | 110 | Epic | head | 20.0 | report-only | socket budget counted; socket colors and socket bonus unchanged; armor median from 10 sample(s) within 10 ilvl |
| 32089 | Mana-Binders Cowl | 110 | Epic | head | 20.0 | report-only | socket budget counted; socket colors and socket bonus unchanged; armor median from 14 sample(s) within 10 ilvl |
| 32090 | Cowl of Naaru Blessings | 110 | Epic | head | 20.0 | report-only | socket budget counted; socket colors and socket bonus unchanged; armor median from 14 sample(s) within 10 ilvl |
| 33808 | The Horseman's Helm | 110 | Epic | head | 24.0 | report-only | socket budget counted; socket colors and socket bonus unchanged; spell payload ignored and left unchanged; armor median from 13 sample(s) within 10 ilvl |
| 34625 | Kharmaa's Ring of Fate | 110 | Epic | finger | 8.0 | report-only | socket budget counted; socket colors and socket bonus unchanged |
| 34799 | Hauberk of the War Bringer | 110 | Epic | chest | 24.0 | report-only | socket budget counted; socket colors and socket bonus unchanged; armor median from 13 sample(s) within 10 ilvl |
| 34807 | Sunstrider Warboots | 110 | Epic | feet | 16.0 | report-only | socket budget counted; socket colors and socket bonus unchanged; armor median from 8 sample(s) within 10 ilvl |
| 34808 | Gloves of Arcane Acuity | 110 | Epic | hands | 16.0 | report-only | socket budget counted; socket colors and socket bonus unchanged; armor median from 12 sample(s) within 10 ilvl |
| 34809 | Sunrage Treads | 110 | Epic | feet | 8.0 | report-only | socket budget counted; socket colors and socket bonus unchanged; armor median from 8 sample(s) within 10 ilvl |
| 23510 | Enchanted Adamantite Belt | 113 | Rare | waist | 16.0 | report-only | socket budget counted; socket colors and socket bonus unchanged; itemset ignored and left unchanged; armor median from 9 sample(s) within 10 ilvl |
| 23511 | Enchanted Adamantite Boots | 113 | Rare | feet | 16.0 | report-only | socket budget counted; socket colors and socket bonus unchanged; itemset ignored and left unchanged; armor median from 10 sample(s) within 10 ilvl |
| 23516 | Flamebane Helm | 113 | Rare | head | 24.0 | report-only | socket budget counted; socket colors and socket bonus unchanged; itemset ignored and left unchanged; armor median from 25 sample(s) within 10 ilvl |
| 25690 | Heavy Clefthoof Leggings | 113 | Rare | legs | 24.0 | report-only | socket budget counted; socket colors and socket bonus unchanged; itemset ignored and left unchanged; armor median from 34 sample(s) within 10 ilvl |
| 28244 | Pendant of Triumph | 113 | Epic | neck | 8.0 | report-only | socket budget counted; socket colors and socket bonus unchanged |
| 28245 | Pendant of Dominance | 113 | Epic | neck | 8.0 | report-only | socket budget counted; socket colors and socket bonus unchanged |
| 28381 | General's Plate Bracers | 113 | Epic | wrist | 8.0 | report-only | socket budget counted; socket colors and socket bonus unchanged; armor median from 15 sample(s) within 10 ilvl |

## Spell Payloads Intentionally Ignored

| entry | name | item_level | quality_label | slot_key | action | warnings |
| --- | --- | --- | --- | --- | --- | --- |
| 29269 | Sapphiron's Wing Bone | 110 | Epic | held_in_off_hand | report-only | spell payload ignored and left unchanged |
| 29270 | Flametongue Seal | 110 | Epic | held_in_off_hand | report-only | spell payload ignored and left unchanged |
| 29271 | Talisman of Kalecgos | 110 | Epic | held_in_off_hand | report-only | spell payload ignored and left unchanged |
| 29272 | Orb of the Soul-Eater | 110 | Epic | held_in_off_hand | report-only | spell payload ignored and left unchanged |
| 29347 | Talisman of the Breaker | 110 | Epic | neck | report-only | spell payload ignored and left unchanged |
| 29357 | Master Thief's Gloves | 110 | Epic | hands | report-only | spell payload ignored and left unchanged; armor median from 12 sample(s) within 10 ilvl |
| 29370 | Icon of the Silver Crescent | 110 | Epic |  | skipped | spell payload ignored and left unchanged |
| 29376 | Essence of the Martyr | 110 | Epic |  | skipped | spell payload ignored and left unchanged |
| 29383 | Bloodlust Brooch | 110 | Epic |  | skipped | spell payload ignored and left unchanged |
| 29387 | Gnomeregan Auto-Blocker 600 | 110 | Epic |  | skipped | spell payload ignored and left unchanged |
| 33808 | The Horseman's Helm | 110 | Epic | head | report-only | socket budget counted; socket colors and socket bonus unchanged; spell payload ignored and left unchanged; armor median from 13 sample(s) within 10 ilvl |
| 35514 | Frostscythe of Lord Ahune | 110 | Epic | weapon_two_hand | report-only | spell payload ignored and left unchanged; weapon DPS median from 4 sample(s) within 10 ilvl |
| 38287 | Empty Mug of Direbrew | 110 | Epic |  | skipped | spell payload ignored and left unchanged |
| 38288 | Direbrew Hops | 110 | Epic |  | skipped | spell payload ignored and left unchanged |
| 38289 | Coren's Lucky Coin | 110 | Epic |  | skipped | spell payload ignored and left unchanged |
| 38290 | Dark Iron Smoking Pipe | 110 | Epic |  | skipped | spell payload ignored and left unchanged |
| 828042 | Pyromantic Sigil | 110 | Epic |  | skipped | spell payload ignored and left unchanged |
| 829383 | Vial of Boiling Blood | 110 | Epic |  | skipped | spell payload ignored and left unchanged |
| 24080 | Khorium Band of Frost | 113 | Rare | finger | report-only | spell payload ignored and left unchanged |
| 24082 | Khorium Inferno Band | 113 | Rare | finger | report-only | spell payload ignored and left unchanged |
| 24106 | Thick Felsteel Necklace | 113 | Rare | neck | report-only | spell payload ignored and left unchanged |
| 24110 | Living Ruby Pendant | 113 | Rare | neck | report-only | spell payload ignored and left unchanged |
| 24085 | Khorium Band of Leaves | 114 | Rare | finger | report-only | spell payload ignored and left unchanged |
| 24092 | Pendant of Frozen Flame | 114 | Rare | neck | report-only | spell payload ignored and left unchanged |
| 24093 | Pendant of Thawing | 114 | Rare | neck | report-only | spell payload ignored and left unchanged |
| 24095 | Pendant of Withering | 114 | Rare | neck | report-only | spell payload ignored and left unchanged |
| 24097 | Pendant of Shadow's End | 114 | Rare | neck | report-only | spell payload ignored and left unchanged |
| 24098 | Pendant of the Null Rune | 114 | Rare | neck | report-only | spell payload ignored and left unchanged |
| 24116 | Eye of the Night | 114 | Rare | neck | report-only | spell payload ignored and left unchanged |
| 24086 | Arcane Khorium Band | 115 | Rare | finger | report-only | spell payload ignored and left unchanged |

## Itemsets Intentionally Ignored

| entry | name | item_level | quality_label | slot_key | action | warnings |
| --- | --- | --- | --- | --- | --- | --- |
| 23510 | Enchanted Adamantite Belt | 113 | Rare | waist | report-only | socket budget counted; socket colors and socket bonus unchanged; itemset ignored and left unchanged; armor median from 9 sample(s) within 10 ilvl |
| 23511 | Enchanted Adamantite Boots | 113 | Rare | feet | report-only | socket budget counted; socket colors and socket bonus unchanged; itemset ignored and left unchanged; armor median from 10 sample(s) within 10 ilvl |
| 23516 | Flamebane Helm | 113 | Rare | head | report-only | socket budget counted; socket colors and socket bonus unchanged; itemset ignored and left unchanged; armor median from 25 sample(s) within 10 ilvl |
| 25690 | Heavy Clefthoof Leggings | 113 | Rare | legs | report-only | socket budget counted; socket colors and socket bonus unchanged; itemset ignored and left unchanged; armor median from 34 sample(s) within 10 ilvl |
| 21861 | Imbued Netherweave Robe | 114 | Rare | chest | report-only | itemset ignored and left unchanged; armor median from 29 sample(s) within 10 ilvl |
| 21862 | Imbued Netherweave Tunic | 114 | Rare | chest | report-only | itemset ignored and left unchanged; armor unchanged: not enough comparable armor samples |
| 21867 | Arcanoweave Boots | 114 | Rare | feet | report-only | itemset ignored and left unchanged; armor median from 13 sample(s) within 10 ilvl |
| 23509 | Enchanted Adamantite Breastplate | 114 | Rare | chest | report-only | socket budget counted; socket colors and socket bonus unchanged; itemset ignored and left unchanged; armor median from 25 sample(s) within 10 ilvl |
| 23514 | Flamebane Gloves | 114 | Rare | hands | report-only | socket budget counted; socket colors and socket bonus unchanged; itemset ignored and left unchanged; armor median from 27 sample(s) within 10 ilvl |
| 23517 | Felsteel Gloves | 114 | Rare | hands | report-only | socket budget counted; socket colors and socket bonus unchanged; itemset ignored and left unchanged; armor median from 27 sample(s) within 10 ilvl |
| 23518 | Felsteel Leggings | 114 | Rare | legs | report-only | socket budget counted; socket colors and socket bonus unchanged; itemset ignored and left unchanged; armor median from 24 sample(s) within 10 ilvl |
| 23523 | Khorium Pants | 114 | Rare | legs | report-only | itemset ignored and left unchanged; armor median from 24 sample(s) within 10 ilvl |
| 23524 | Khorium Belt | 114 | Rare | waist | report-only | itemset ignored and left unchanged; armor median from 9 sample(s) within 10 ilvl |
| 25689 | Heavy Clefthoof Vest | 114 | Rare | chest | report-only | socket budget counted; socket colors and socket bonus unchanged; itemset ignored and left unchanged; armor median from 17 sample(s) within 10 ilvl |
| 25696 | Felstalker Breastplate | 114 | Rare | chest | report-only | socket budget counted; socket colors and socket bonus unchanged; itemset ignored and left unchanged; armor median from 19 sample(s) within 10 ilvl |
| 25697 | Felstalker Bracers | 114 | Rare | wrist | report-only | socket budget counted; socket colors and socket bonus unchanged; itemset ignored and left unchanged; armor median from 6 sample(s) within 10 ilvl |
| 21868 | Arcanoweave Robe | 115 | Rare | chest | report-only | itemset ignored and left unchanged; armor median from 29 sample(s) within 10 ilvl |
| 21873 | Primal Mooncloth Belt | 115 | Epic | waist | report-only | socket budget counted; socket colors and socket bonus unchanged; itemset ignored and left unchanged; armor median from 18 sample(s) within 10 ilvl |
| 23512 | Enchanted Adamantite Leggings | 115 | Rare | legs | report-only | socket budget counted; socket colors and socket bonus unchanged; itemset ignored and left unchanged; armor median from 24 sample(s) within 10 ilvl |
| 23513 | Flamebane Breastplate | 115 | Rare | chest | report-only | socket budget counted; socket colors and socket bonus unchanged; itemset ignored and left unchanged; armor median from 25 sample(s) within 10 ilvl |
| 23519 | Felsteel Helm | 115 | Rare | head | report-only | socket budget counted; socket colors and socket bonus unchanged; itemset ignored and left unchanged; armor median from 25 sample(s) within 10 ilvl |
| 23520 | Ragesteel Gloves | 115 | Rare | hands | report-only | itemset ignored and left unchanged; armor median from 27 sample(s) within 10 ilvl |
| 23521 | Ragesteel Helm | 115 | Rare | head | report-only | itemset ignored and left unchanged; armor median from 25 sample(s) within 10 ilvl |
| 23522 | Ragesteel Breastplate | 115 | Rare | chest | report-only | itemset ignored and left unchanged; armor median from 25 sample(s) within 10 ilvl |
| 23525 | Khorium Boots | 115 | Rare | feet | report-only | itemset ignored and left unchanged; armor median from 9 sample(s) within 10 ilvl |
| 27465 | Mana-Etched Gloves | 115 | Rare | hands | report-only | socket budget counted; socket colors and socket bonus unchanged; itemset ignored and left unchanged; armor median from 22 sample(s) within 10 ilvl |
| 27468 | Moonglade Handwraps | 115 | Rare | hands | report-only | itemset ignored and left unchanged; armor median from 24 sample(s) within 10 ilvl |
| 27474 | Beast Lord Handguards | 115 | Rare | hands | report-only | socket budget counted; socket colors and socket bonus unchanged; itemset ignored and left unchanged; armor median from 25 sample(s) within 10 ilvl |
| 27475 | Gauntlets of the Bold | 115 | Rare | hands | report-only | socket budget counted; socket colors and socket bonus unchanged; itemset ignored and left unchanged; armor median from 27 sample(s) within 10 ilvl |
| 27497 | Doomplate Gauntlets | 115 | Rare | hands | report-only | socket budget counted; socket colors and socket bonus unchanged; itemset ignored and left unchanged; armor median from 27 sample(s) within 10 ilvl |

## Unknown Stat Types

None.

## Calibration Snapshot

Grouped by required level, item level, quality, inventory type, subclass, slot, and inferred role. Full data is in the calibration CSV.

| count | required_level | item_level | quality_label | inventory_label | subclass_label | role | median_visible_budget | median_total_budget_with_sockets | median_target_budget | median_percent_over_under |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 25 | 70 | 115 | Rare | Robe | Cloth | caster | 89.35 | 109.77 | 49.323 | 122.552 |
| 23 | 70 | 115 | Rare | Head | Cloth | caster | 87.61 | 106.09 | 49.323 | 115.091 |
| 21 | 70 | 115 | Rare | Shoulder | Cloth | caster | 70.0 | 86.0 | 36.499 | 135.622 |
| 21 | 70 | 115 | Rare | Legs | Cloth | caster | 110.1 | 110.1 | 49.323 | 123.221 |
| 17 | 70 | 115 | Rare | Hands | Cloth | caster | 73.33 | 76.13 | 36.499 | 108.58 |
| 17 | 70 | 115 | Rare | Finger | Misc | caster | 51.05 | 51.05 | 27.621 | 84.823 |
| 13 | 70 | 115 | Rare | Chest | Plate | generic | 94.14 | 118.14 | 49.323 | 139.522 |
| 12 | 70 | 115 | Rare | Legs | Mail | caster | 123.0 | 128.115 | 49.323 | 159.746 |
| 11 | 70 | 115 | Rare | Legs | Leather | caster | 122.65 | 122.65 | 49.323 | 148.666 |
| 11 | 70 | 115 | Rare | Hands | Mail | caster | 85.26 | 85.26 | 36.499 | 133.594 |
| 10 | 70 | 115 | Rare | Shoulder | Leather | caster | 74.65 | 90.65 | 36.499 | 148.362 |
| 10 | 70 | 115 | Rare | Shoulder | Mail | caster | 84.51 | 100.51 | 36.499 | 175.376 |
| 10 | 70 | 115 | Rare | Legs | Leather | physical | 102.47 | 109.125 | 49.323 | 121.244 |
| 10 | 70 | 115 | Rare | Hands | Leather | caster | 85.26 | 85.26 | 36.499 | 133.594 |
| 10 | 70 | 115 | Rare | Hands | Leather | physical | 79.77 | 79.77 | 36.499 | 118.553 |
| 10 | 70 | 115 | Rare | Cloak | Cloth | caster | 54.805 | 56.05 | 27.621 | 102.925 |
| 10 | 70 | 115 | Rare | Two-hand Weapon | Staff | caster | 200.34 | 202.19 | 88.782 | 127.738 |
| 10 | 70 | 115 | Rare | Held in Off-hand | Misc | caster | 53.69 | 53.69 | 27.621 | 94.381 |
| 10 | 70 | 141 | Epic | Neck | Misc | caster | 96.27 | 104.27 | 40.155 | 159.67 |
| 10 | 70 | 141 | Epic | Finger | Misc | caster | 97.81 | 97.81 | 40.155 | 143.583 |
| 9 | 70 | 115 | Rare | Head | Leather | caster | 96.88 | 116.88 | 49.323 | 136.967 |
| 9 | 70 | 115 | Rare | Head | Plate | generic | 91.1 | 111.1 | 49.323 | 125.249 |
| 9 | 70 | 115 | Rare | Shoulder | Plate | generic | 74.11 | 90.11 | 36.499 | 146.882 |
| 9 | 70 | 115 | Rare | Chest | Mail | caster | 106.9 | 120.6 | 49.323 | 144.509 |
| 9 | 70 | 115 | Rare | Legs | Plate | generic | 112.14 | 112.14 | 49.323 | 127.357 |
| 9 | 70 | 115 | Rare | Hands | Plate | caster | 86.18 | 89.04 | 36.499 | 143.95 |
| 9 | 70 | 123 | Epic | Head | Cloth | caster | 117.72 | 137.72 | 61.283 | 124.729 |
| 9 | 70 | 123 | Epic | Shoulder | Cloth | caster | 102.66 | 118.66 | 45.349 | 161.658 |
| 9 | 70 | 123 | Epic | Legs | Cloth | caster | 155.3 | 155.3 | 61.283 | 153.416 |
| 9 | 70 | 123 | Epic | Hands | Cloth | caster | 99.65 | 99.65 | 45.349 | 119.739 |
| 9 | 70 | 141 | Epic | Wrist | Cloth | caster | 93.21 | 95.29 | 40.155 | 137.307 |
| 9 | 70 | 146 | Epic | Head | Cloth | caster | 159.95 | 179.95 | 74.637 | 141.101 |
| 9 | 70 | 146 | Epic | Shoulder | Cloth | caster | 120.9 | 136.9 | 55.231 | 147.867 |
| 9 | 70 | 146 | Epic | Legs | Cloth | caster | 182.11 | 182.11 | 74.637 | 143.995 |
| 9 | 70 | 146 | Epic | Hands | Cloth | caster | 130.9 | 130.9 | 55.231 | 137.004 |
| 9 | 70 | 159 | Epic | Robe | Cloth | caster | 175.1 | 199.1 | 82.329 | 141.834 |
| 8 | 70 | 115 | Rare | Head | Mail | caster | 94.59 | 114.59 | 49.323 | 132.324 |
| 8 | 70 | 115 | Rare | Neck | Misc | caster | 59.66 | 59.66 | 27.621 | 115.995 |
| 8 | 70 | 115 | Rare | Shoulder | Mail | generic | 78.59 | 91.11 | 36.499 | 149.622 |
| 8 | 70 | 115 | Rare | Finger | Misc | physical | 53.905 | 53.905 | 27.621 | 95.159 |
