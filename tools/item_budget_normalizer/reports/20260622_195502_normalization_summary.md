# Item Budget Normalizer Summary

- Generated: 2026-06-22T19:55:03
- Input CSV: `F:\Downloads\First_pass_normalization.csv`
- Config: `C:\AzerothCore\Burning_ Legacy_Server\AzerothCore-wotlk-with-NPCBots\tools\item_budget_normalizer\item_budget_config.yml`
- SQL generation: disabled (report-only dry run)
- Target mode: `empirical`
- Calibration sanity: sane; worst grouped median `0.0%`
- Filters: min ilvl `None`, max ilvl `None`, required level `None`, qualities `any`

## Output Files

- Before/after CSV: `tools\item_budget_normalizer\reports\20260622_195502_normalization_report.csv`
- Calibration CSV: `tools\item_budget_normalizer\reports\20260622_195502_calibration.csv`
- Update SQL: not generated
- Rollback SQL: not generated

## Counts

- Report rows: 18347
- Skipped: 620
- Report-only changes: 14282
- Updated SQL rows: 0
- Unchanged: 3445
- Would change if SQL were enabled: 14282
- Special weapon-stat rows: 1670

## Skipped Items And Why

| Reason | Count |
| --- | --- |
| cannot classify inventory slot | 505 |
| trinket excluded | 459 |
| not armor or weapon class | 58 |
| name matched exclusion pattern Test  | 36 |
| quality 1 not configured | 28 |
| name matched exclusion pattern [PH] | 11 |
| duplicate stat type 32 in slot 4 | 6 |
| non-contiguous stat slot 4 | 5 |
| duplicate stat type 38 in slot 7 | 4 |
| quality 6 not configured | 4 |
| stat slot 1 has negative value -5 | 4 |
| duplicate stat type 32 in slot 7 | 3 |
| name matched exclusion pattern Monster - | 3 |
| stat slot 1 has negative value -3 | 3 |
| stat slot 1 has negative value -30 | 3 |
| stat slot 2 has negative value -10 | 3 |
| stat slot 2 has negative value -5 | 3 |
| duplicate stat type 38 in slot 3 | 2 |
| duplicate stat type 45 in slot 5 | 2 |
| duplicate stat type 45 in slot 6 | 2 |
| non-contiguous stat slot 3 | 2 |
| quality 7 not configured | 2 |
| scaling item excluded | 2 |
| stat slot 3 has negative value -10 | 2 |
| ammo/projectile excluded | 1 |
| duplicate stat type 31 in slot 6 | 1 |
| duplicate stat type 32 in slot 2 | 1 |
| duplicate stat type 38 in slot 2 | 1 |
| duplicate stat type 38 in slot 4 | 1 |
| name matched exclusion pattern QA | 1 |
| no normal stats and no weapon damage | 1 |
| non-contiguous stat slot 5 | 1 |
| non-contiguous stat slot 6 | 1 |
| shirt excluded | 1 |
| stat slot 1 has negative value -10 | 1 |
| stat slot 1 has negative value -15 | 1 |
| stat slot 2 has negative value -3 | 1 |
| stat slot 3 has negative value -35 | 1 |
| stat slot 3 has negative value -40 | 1 |
| stat slot 3 has negative value -42 | 1 |

## Most Overbudget Before Normalization

| entry | name | item_level | quality_label | slot_key | percent_over_under_before | action |
| --- | --- | --- | --- | --- | --- | --- |
| 8350 | The 1 Ring | 15 | Uncommon | finger | 367.0 | unchanged |
| 13944 | Tombstone Breastplate | 62 | Rare | chest | 344.776 | report-only |
| 32955 | Tom's Boots 2 | 130 | Epic | feet | 322.276 | report-only |
| 32958 | Tom's Bracer 2 | 130 | Epic | wrist | 316.866 | report-only |
| 32418 | Tom's Legs 2 | 130 | Epic | legs | 315.229 | report-only |
| 32954 | Tom's Boots 1 | 115 | Epic | feet | 301.368 | report-only |
| 21616 | Huhuran's Stinger | 78 | Epic | ranged | 283.795 | report-only |
| 12936 | Battleborn Armbraces | 63 | Rare | wrist | 258.209 | report-only |
| 31802 | Fleshling Simulation Staff | 109 | Rare | weapon_one_hand | 250.071 | report-only |
| 24116 | Eye of the Night | 114 | Rare | neck | 248.259 | report-only |
| 43069 | Blessed Breastplate of Undead Slaying | 115 | Epic | chest | 231.288 | report-only |
| 23229 | Sword of Sockety Goodness | 41 | Uncommon | weapon_one_hand | 217.687 | report-only |
| 25804 | Naliko's Revenge | 109 | Rare | finger | 193.477 | report-only |
| 25811 | Conqueror's Band | 109 | Rare | finger | 193.477 | report-only |
| 32417 | Tom's Legs 1 | 115 | Epic | legs | 185.961 | report-only |
| 22815 | Severance | 81 | Epic | weapon_two_hand | 185.577 | report-only |
| 7515 | Celestial Orb | 40 | Rare | held_in_off_hand | 183.6 | report-only |
| 19559 | Outrider's Bow | 53 | Rare | ranged | 178.667 | report-only |
| 19563 | Outrunner's Bow | 53 | Rare | ranged | 178.667 | report-only |
| 13075 | Direwing Legguards | 63 | Rare | legs | 161.262 | report-only |

## Most Underbudget Before Normalization

| entry | name | item_level | quality_label | slot_key | percent_over_under_before | action |
| --- | --- | --- | --- | --- | --- | --- |
| 30504 | Leafblade Dagger | 19 | Uncommon | weapon_one_hand | -100.0 | report-only |
| 23411 | Well Crafted Staff | 20 | Uncommon | weapon_two_hand | -100.0 | unchanged |
| 5191 | Cruel Barb | 24 | Rare | weapon_one_hand | -100.0 | report-only |
| 16886 | Outlaw Sabre | 30 | Rare | weapon_one_hand | -100.0 | report-only |
| 12259 | Glinting Steel Dagger | 36 | Uncommon | weapon_one_hand | -100.0 | report-only |
| 1664 | Spellforce Rod | 41 | Uncommon | weapon_two_hand | -100.0 | unchanged |
| 4983 | Rock Pulverizer | 42 | Uncommon | weapon_two_hand | -100.0 | report-only |
| 10823 | Vanquisher's Sword | 44 | Rare | weapon_one_hand | -100.0 | report-only |
| 12527 | Ribsplitter | 53 | Rare | weapon_one_hand | -100.0 | report-only |
| 17717 | Megashot Rifle | 53 | Rare | ranged | -100.0 | report-only |
| 11923 | The Hammer of Grace | 57 | Rare | weapon_main_hand | -100.0 | report-only |
| 12061 | Blade of Reckoning | 60 | Uncommon | weapon_one_hand | -100.0 | report-only |
| 18484 | Cho'Rush's Blade | 61 | Rare | weapon_one_hand | -100.0 | report-only |
| 13368 | Bonescraper | 62 | Rare | weapon_one_hand | -100.0 | report-only |
| 20675 | Soulrender | 62 | Rare | weapon_one_hand | -100.0 | report-only |
| 17073 | Earthshaker | 66 | Epic | weapon_two_hand | -100.0 | report-only |
| 19866 | Warblade of the Hakkari | 66 | Epic | weapon_off_hand | -100.0 | unchanged |
| 25324 | Angerstaff | 81 | Uncommon | weapon_two_hand | -100.0 | report-only |
| 25327 | Frenzied Staff | 90 | Uncommon | weapon_two_hand | -100.0 | report-only |
| 25300 | Lightning Dagger | 93 | Uncommon | weapon_main_hand | -100.0 | report-only |

## Calibration Sanity Offenders

None.

## Weapons Changed

| entry | name | item_level | quality_label | subclass_label | old_dps | new_dps | action | warnings |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 1917 | Jeweled Dagger | 10 | Uncommon | Dagger | 5.312 | 7.812 | report-only | visible stat target clamped to +/-20% safety cap; target visible budget raised by configured minimum; rounding remained outside configured tolerance; weapon DPS median from 33 sample(s) within 10 ilvl |
| 9513 | Ley Staff | 10 | Uncommon | Staff | 7.069 | 11.379 | report-only | weapon DPS median from 17 sample(s) within 10 ilvl |
| 9514 | Arcane Staff | 10 | Uncommon | Staff | 7.069 | 11.379 | report-only | weapon DPS median from 17 sample(s) within 10 ilvl |
| 9602 | Brushwood Blade | 10 | Uncommon | Two-hand Sword | 7.167 | 11.333 | report-only | visible stat target clamped to +/-20% safety cap; target visible budget raised by configured minimum; rounding remained outside configured tolerance; weapon DPS median from 15 sample(s) within 10 ilvl |
| 9603 | Gritroot Staff | 10 | Uncommon | Staff | 6.923 | 11.346 | report-only | visible stat target clamped to +/-20% safety cap; target visible budget raised by configured minimum; rounding remained outside configured tolerance; weapon DPS median from 17 sample(s) within 10 ilvl |
| 18957 | Brushwood Blade | 10 | Uncommon | Sword | 5.333 | 7.667 | report-only | visible stat target clamped to +/-20% safety cap; target visible budget raised by configured minimum; rounding remained outside configured tolerance; weapon DPS median from 23 sample(s) within 10 ilvl |
| 23423 | Mercenary Greatsword | 10 | Uncommon | Two-hand Sword | 7.069 | 11.379 | report-only | weapon DPS median from 15 sample(s) within 10 ilvl |
| 23429 | Mercenary Clout | 10 | Uncommon | Two-hand Mace | 7.143 | 9.286 | report-only | weapon DPS median from 10 sample(s) within 10 ilvl |
| 23430 | Mercenary Sword | 10 | Uncommon | Sword | 5.417 | 7.708 | report-only | visible stat target clamped to +/-20% safety cap; target visible budget raised by configured minimum; rounding remained outside configured tolerance; weapon DPS median from 23 sample(s) within 10 ilvl |
| 23431 | Mercenary Stiletto | 10 | Uncommon | Dagger | 5.333 | 7.667 | report-only | visible stat target clamped to +/-20% safety cap; target visible budget raised by configured minimum; rounding remained outside configured tolerance; weapon DPS median from 33 sample(s) within 10 ilvl |
| 816 | Small Hand Blade | 11 | Uncommon | Dagger | 6.0 | 7.667 | report-only | weapon DPS median from 33 sample(s) within 10 ilvl |
| 2281 | Rodentia Flint Axe | 11 | Uncommon | Axe | 6.0 | 7.75 | report-only | weapon DPS median from 12 sample(s) within 10 ilvl |
| 3223 | Frostmane Scepter | 11 | Uncommon | Mace | 5.909 | 7.955 | report-only | visible stat target clamped to +/-20% safety cap; target visible budget raised by configured minimum; rounding remained outside configured tolerance; weapon DPS median from 28 sample(s) within 10 ilvl |
| 4939 | Steady Bastard Sword | 11 | Uncommon | Two-hand Sword | 7.593 | 11.481 | report-only | weapon DPS median from 15 sample(s) within 10 ilvl |
| 4947 | Jagged Dagger | 11 | Uncommon | Dagger | 6.0 | 7.667 | report-only | weapon DPS median from 33 sample(s) within 10 ilvl |
| 4948 | Stinging Mace | 11 | Uncommon | Mace | 5.87 | 7.826 | report-only | weapon DPS median from 28 sample(s) within 10 ilvl |
| 24339 | Stung | 11 | Uncommon | Sword | 6.0 | 8.2 | report-only | weapon DPS median from 25 sample(s) within 10 ilvl |
| 26049 | Old Elekk Prod | 11 | Uncommon | Mace | 5.833 | 8.056 | report-only | visible stat target clamped to +/-20% safety cap; target visible budget raised by configured minimum; rounding remained outside configured tolerance; weapon DPS median from 28 sample(s) within 10 ilvl |
| 26051 | 2 Stone Sledgehammer | 11 | Uncommon | Two-hand Mace | 7.714 | 9.857 | report-only | rounding remained outside configured tolerance; weapon DPS median from 12 sample(s) within 10 ilvl |
| 26053 | Elekk Handler's Blade | 11 | Uncommon | Sword | 6.0 | 8.25 | report-only | weapon DPS median from 25 sample(s) within 10 ilvl |
| 27389 | Surplus Bastard Sword | 11 | Uncommon | Two-hand Sword | 7.812 | 11.406 | report-only | weapon DPS median from 15 sample(s) within 10 ilvl |
| 33791 | Heavy Copper Longsword | 11 | Uncommon | Sword | 6.0 | 8.2 | report-only | weapon DPS median from 25 sample(s) within 10 ilvl |
| 2254 | Icepane Warhammer | 12 | Uncommon | Two-hand Mace | 8.485 | 9.697 | report-only | weapon DPS median from 12 sample(s) within 10 ilvl |
| 3103 | Coldridge Hammer | 12 | Uncommon | Two-hand Mace | 8.333 | 9.815 | report-only | weapon DPS median from 12 sample(s) within 10 ilvl |
| 4303 | Cranial Thumper | 12 | Uncommon | Mace | 6.429 | 8.036 | report-only | weapon DPS median from 31 sample(s) within 10 ilvl |
| 4964 | Goblin Smasher | 12 | Uncommon | Two-hand Mace | 8.226 | 9.839 | report-only | weapon DPS median from 12 sample(s) within 10 ilvl |
| 4971 | Skorn's Hammer | 12 | Uncommon | Mace | 6.481 | 8.148 | report-only | weapon DPS median from 31 sample(s) within 10 ilvl |
| 4974 | Compact Fighting Knife | 12 | Uncommon | Dagger | 6.333 | 7.667 | report-only | weapon DPS median from 35 sample(s) within 10 ilvl |
| 2218 | Craftsman's Dagger | 13 | Uncommon | Dagger | 6.765 | 8.235 | report-only | weapon DPS median from 36 sample(s) within 10 ilvl |
| 2265 | Stonesplinter Axe | 13 | Uncommon | Axe | 6.818 | 7.727 | report-only | weapon DPS median from 14 sample(s) within 10 ilvl |

## Armor Changed

| entry | name | item_level | quality_label | subclass_label | old_armor | new_armor | action | warnings |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 16604 | Moon Robes of Elune | 5 | Uncommon | Cloth | 10 | 19 | report-only | armor median from 17 sample(s) within 10 ilvl |
| 16605 | Friar's Robes of the Light | 5 | Uncommon | Cloth | 10 | 19 | report-only | armor median from 17 sample(s) within 10 ilvl |
| 16606 | Juju Hex Robes | 5 | Uncommon | Cloth | 10 | 19 | report-only | armor median from 17 sample(s) within 10 ilvl |
| 16607 | Acolyte's Sacrificial Robes | 5 | Uncommon | Cloth | 10 | 19 | report-only | armor median from 17 sample(s) within 10 ilvl |
| 23924 | Robes of Silvermoon | 5 | Uncommon | Cloth | 10 | 19 | report-only | armor median from 17 sample(s) within 10 ilvl |
| 23931 | Azure Watch Robes | 5 | Uncommon | Cloth | 10 | 19 | report-only | armor median from 17 sample(s) within 10 ilvl |
| 2572 | Red Linen Robe | 10 | Uncommon | Cloth | 19 | 26 | report-only | armor median from 27 sample(s) within 10 ilvl |
| 2958 | Journeyman's Pants | 10 | Uncommon | Cloth | 17 | 26 | report-only | armor median from 22 sample(s) within 10 ilvl |
| 2962 | Burnt Leather Breeches | 10 | Uncommon | Leather | 48 | 64 | report-only | armor median from 27 sample(s) within 10 ilvl |
| 2966 | Warrior's Pants | 10 | Uncommon | Mail | 94 | 132 | report-only | visible stat target clamped to +/-20% safety cap; target visible budget raised by configured minimum; rounding remained outside configured tolerance; armor median from 17 sample(s) within 10 ilvl |
| 3000 | Brood Mother Carapace | 10 | Uncommon | Leather | 55 | 70 | report-only | visible stat target clamped to +/-20% safety cap; target visible budget raised by configured minimum; rounding remained outside configured tolerance; armor median from 27 sample(s) within 10 ilvl |
| 3471 | Copper Chain Vest | 10 | Uncommon | Mail | 108 | 144 | report-only | armor median from 19 sample(s) within 10 ilvl |
| 4861 | Sleek Feathered Tunic | 10 | Uncommon | Leather | 55 | 70 | report-only | armor median from 27 sample(s) within 10 ilvl |
| 6238 | Brown Linen Robe | 10 | Uncommon | Cloth | 19 | 26 | report-only | armor median from 27 sample(s) within 10 ilvl |
| 6241 | White Linen Robe | 10 | Uncommon | Cloth | 19 | 26 | report-only | armor median from 27 sample(s) within 10 ilvl |
| 9598 | Sleeping Robes | 10 | Uncommon | Cloth | 19 | 26 | report-only | visible stat target clamped to +/-20% safety cap; target visible budget raised by configured minimum; rounding remained outside configured tolerance; armor median from 27 sample(s) within 10 ilvl |
| 9599 | Barkmail Leggings | 10 | Uncommon | Mail | 94 | 132 | report-only | visible stat target clamped to +/-20% safety cap; target visible budget raised by configured minimum; rounding remained outside configured tolerance; armor median from 17 sample(s) within 10 ilvl |
| 17922 | Lionfur Armor | 10 | Uncommon | Leather | 55 | 70 | report-only | armor median from 27 sample(s) within 10 ilvl |
| 23399 | Fallen Apprentice's Robe | 10 | Uncommon | Cloth | 19 | 26 | report-only | visible stat target clamped to +/-20% safety cap; rounding remained outside configured tolerance; armor median from 27 sample(s) within 10 ilvl |
| 24107 | Ravager Chitin Tunic | 10 | Uncommon | Mail | 108 | 144 | report-only | armor median from 19 sample(s) within 10 ilvl |
| 24108 | Ravager Hide Leggings | 10 | Uncommon | Leather | 48 | 64 | report-only | armor median from 27 sample(s) within 10 ilvl |
| 24109 | Thick Ravager Belt | 10 | Uncommon | Cloth | 11 | 17 | report-only | armor median from 14 sample(s) within 10 ilvl |
| 24111 | Kurken Hide Jerkin | 10 | Uncommon | Leather | 55 | 70 | report-only | armor median from 27 sample(s) within 10 ilvl |
| 24112 | Kurkenstoks | 10 | Uncommon | Cloth | 13 | 21 | report-only | visible stat target clamped to +/-20% safety cap; target visible budget raised by configured minimum; rounding remained outside configured tolerance; armor median from 11 sample(s) within 10 ilvl |
| 24113 | Cowlen's Bracers of Kinship | 10 | Uncommon | Cloth | 8 | 14 | report-only | visible stat target clamped to +/-20% safety cap; target visible budget raised by configured minimum; rounding remained outside configured tolerance; armor median from 9 sample(s) within 10 ilvl |
| 28146 | Courier's Wraps | 10 | Uncommon | Cloth | 8 | 14 | report-only | armor median from 9 sample(s) within 10 ilvl |
| 28147 | Tranquillien Scout's Bracers | 10 | Uncommon | Leather | 24 | 33 | report-only | armor median from 7 sample(s) within 10 ilvl |
| 28148 | Bronze Mail Bracers | 10 | Uncommon | Mail | 47 | 71 | report-only | armor median from 5 sample(s) within 10 ilvl |
| 2957 | Journeyman's Vest | 11 | Uncommon | Cloth | 21 | 26 | report-only | armor median from 12 sample(s) within 10 ilvl |
| 2961 | Burnt Leather Vest | 11 | Uncommon | Leather | 58 | 70 | report-only | armor median from 28 sample(s) within 10 ilvl |

## Items With Sockets

| entry | name | item_level | quality_label | slot_key | socket_budget | action | warnings |
| --- | --- | --- | --- | --- | --- | --- | --- |
| 23229 | Sword of Sockety Goodness | 41 | Uncommon | weapon_one_hand | 16.0 | report-only | socket budget counted; socket colors and socket bonus unchanged; visible stat target clamped to +/-20% safety cap; target visible budget raised by configured minimum; rounding remained outside configured tolerance; final visible stat budget adjusted to stay within +/-20% safety cap |
| 27830 | Circlet of the Victor | 80 | Rare | finger | 8.0 | unchanged | socket budget counted; socket colors and socket bonus unchanged |
| 27832 | Band of the Victor | 80 | Rare | finger | 8.0 | unchanged | socket budget counted; socket colors and socket bonus unchanged |
| 27833 | Band of the Victor | 80 | Rare | finger | 8.0 | unchanged | socket budget counted; socket colors and socket bonus unchanged |
| 27834 | Circlet of the Victor | 80 | Rare | finger | 8.0 | unchanged | socket budget counted; socket colors and socket bonus unchanged |
| 24021 | Light-Touched Breastplate | 85 | Rare | chest | 24.0 | report-only | socket budget counted; socket colors and socket bonus unchanged; visible stat target clamped to +/-20% safety cap; armor median from 7 sample(s) within 10 ilvl |
| 24022 | Scale Leggings of the Skirmisher | 85 | Rare | legs | 24.0 | report-only | socket budget counted; socket colors and socket bonus unchanged; armor median from 18 sample(s) within 20 ilvl |
| 24046 | Kilt of Rolling Thunders | 85 | Rare | legs | 24.0 | report-only | socket budget counted; socket colors and socket bonus unchanged; armor median from 18 sample(s) within 20 ilvl |
| 24063 | Shifting Sash of Midnight | 85 | Rare | waist | 16.0 | report-only | socket budget counted; socket colors and socket bonus unchanged; armor median from 11 sample(s) within 20 ilvl |
| 24064 | Ironsole Clompers | 85 | Rare | feet | 16.0 | report-only | socket budget counted; socket colors and socket bonus unchanged; armor median from 13 sample(s) within 20 ilvl |
| 24083 | Lifegiver Britches | 85 | Rare | legs | 24.0 | report-only | socket budget counted; socket colors and socket bonus unchanged; armor median from 18 sample(s) within 20 ilvl |
| 24090 | Bloodstained Ravager Gauntlets | 85 | Rare | hands | 16.0 | report-only | socket budget counted; socket colors and socket bonus unchanged; visible stat target clamped to +/-20% safety cap; final visible stat budget adjusted to stay within +/-20% safety cap; armor median from 10 sample(s) within 20 ilvl |
| 24091 | Tenacious Defender | 85 | Rare | waist | 16.0 | report-only | socket budget counted; socket colors and socket bonus unchanged; armor median from 12 sample(s) within 20 ilvl |
| 153959 | Lightdrinker Dagger | 85 | Rare | weapon_one_hand | 8.0 | report-only | special weapon stat budget excluded from normal visible stat budget; socket budget counted; socket colors and socket bonus unchanged; weapon DPS median from 5 sample(s) within 10 ilvl |
| 24387 | Ironblade Gauntlets | 88 | Rare | hands | 16.0 | report-only | socket budget counted; socket colors and socket bonus unchanged; armor median from 8 sample(s) within 20 ilvl |
| 24388 | Girdle of the Gale Storm | 88 | Rare | waist | 16.0 | report-only | socket budget counted; socket colors and socket bonus unchanged; armor median from 8 sample(s) within 20 ilvl |
| 24391 | Kilt of the Night Strider | 88 | Rare | legs | 24.0 | report-only | socket budget counted; socket colors and socket bonus unchanged; armor median from 15 sample(s) within 20 ilvl |
| 24393 | Bloody Surgeon's Mitts | 88 | Rare | hands | 16.0 | report-only | socket budget counted; socket colors and socket bonus unchanged; armor median from 4 sample(s) within 10 ilvl |
| 24395 | Mindfire Waistband | 88 | Rare | waist | 16.0 | report-only | socket budget counted; socket colors and socket bonus unchanged; armor median from 6 sample(s) within 20 ilvl |
| 24396 | Vest of Vengeance | 88 | Rare | chest | 24.0 | report-only | socket budget counted; socket colors and socket bonus unchanged; visible stat target clamped to +/-20% safety cap; armor median from 5 sample(s) within 10 ilvl |
| 24397 | Raiments of Divine Authority | 88 | Rare | chest | 24.0 | report-only | socket budget counted; socket colors and socket bonus unchanged; armor median from 9 sample(s) within 20 ilvl |
| 23362 | Hammer of the Sun | 90 | Epic | weapon_main_hand | 28.0 | unchanged | socket budget counted; socket colors and socket bonus unchanged |
| 23363 | Titanic Breastplate | 90 | Epic | chest | 36.0 | unchanged | socket budget counted; socket colors and socket bonus unchanged |
| 27715 | Circle's Stalwart Helmet | 90 | Uncommon | head | 24.0 | report-only | socket budget counted; socket colors and socket bonus unchanged; armor median from 6 sample(s) within 10 ilvl |
| 27717 | Expedition Forager Leggings | 90 | Uncommon | legs | 24.0 | report-only | socket budget counted; socket colors and socket bonus unchanged; visible stat target clamped to +/-20% safety cap; armor median from 6 sample(s) within 10 ilvl |
| 31657 | Chemise of Rebirth | 90 | Uncommon | chest | 24.0 | report-only | socket budget counted; socket colors and socket bonus unchanged; visible stat target clamped to +/-20% safety cap; armor median from 4 sample(s) within 20 ilvl |
| 31658 | Scout's Hood | 90 | Uncommon | head | 24.0 | report-only | socket budget counted; socket colors and socket bonus unchanged; visible stat target clamped to +/-20% safety cap; armor median from 9 sample(s) within 10 ilvl |
| 24357 | Vest of Living Lightning | 91 | Rare | chest | 24.0 | report-only | socket budget counted; socket colors and socket bonus unchanged; armor median from 7 sample(s) within 10 ilvl |
| 24363 | Unscarred Breastplate | 91 | Rare | chest | 24.0 | report-only | socket budget counted; socket colors and socket bonus unchanged; armor median from 9 sample(s) within 10 ilvl |
| 24365 | Deft Handguards | 91 | Rare | hands | 16.0 | report-only | socket budget counted; socket colors and socket bonus unchanged; armor median from 4 sample(s) within 10 ilvl |

## Spell Payloads Intentionally Ignored

| entry | name | item_level | quality_label | slot_key | action | warnings |
| --- | --- | --- | --- | --- | --- | --- |
| 17 | Martin Fury | 5 | 6 |  | skipped | spell payload ignored and left unchanged; visible stat target clamped to +/-20% safety cap |
| 7507 | Arcane Orb | 10 | Uncommon | held_in_off_hand | unchanged | spell payload ignored and left unchanged |
| 7508 | Ley Orb | 10 | Uncommon | held_in_off_hand | unchanged | spell payload ignored and left unchanged |
| 15866 | Veildust Medicine Bag | 15 | Uncommon | held_in_off_hand | unchanged | spell payload ignored and left unchanged |
| 28164 | Tranquillien Flamberge | 15 | Uncommon | weapon_two_hand | report-only | spell payload ignored and left unchanged; weapon DPS median from 17 sample(s) within 10 ilvl |
| 825438 | Distinct Emerald Pendant | 18 | Rare | neck | unchanged | spell payload ignored and left unchanged |
| 5183 | Pulsating Hydra Heart | 20 | Rare | held_in_off_hand | unchanged | spell payload ignored and left unchanged |
| 5387 | Enchanted Moonstalker Cloak | 20 | Uncommon | cloak | unchanged | spell payload ignored and left unchanged; armor median from 70 sample(s) within 10 ilvl |
| 22783 | Sunwell Blade | 20 | Uncommon | weapon_one_hand | report-only | spell payload ignored and left unchanged; weapon DPS median from 34 sample(s) within 10 ilvl |
| 22784 | Sunwell Orb | 20 | Uncommon | held_in_off_hand | unchanged | spell payload ignored and left unchanged; rounding remained outside configured tolerance |
| 1484 | Witching Stave | 22 | Rare | weapon_two_hand | report-only | spell payload ignored and left unchanged; weapon DPS median from 13 sample(s) within 10 ilvl |
| 5613 | Staff of the Purifier | 23 | Uncommon | weapon_two_hand | report-only | spell payload ignored and left unchanged; weapon DPS median from 24 sample(s) within 10 ilvl |
| 30804 | Bronze Band of Force | 23 | Rare | finger | report-only | spell payload ignored and left unchanged; visible stat target clamped to +/-20% safety cap |
| 4444 | Black Husk Shield | 24 | Uncommon | shield | report-only | spell payload ignored and left unchanged; rounding remained outside configured tolerance; armor median from 45 sample(s) within 10 ilvl |
| 2950 | Icicle Rod | 25 | Uncommon | weapon_two_hand | report-only | spell payload ignored and left unchanged; visible stat target clamped to +/-20% safety cap; rounding remained outside configured tolerance; final visible stat budget adjusted to stay within +/-20% safety cap; weapon DPS median from 21 sample(s) within 10 ilvl |
| 4317 | Phoenix Pants | 25 | Uncommon | legs | report-only | spell payload ignored and left unchanged; armor median from 31 sample(s) within 10 ilvl |
| 5323 | Everglow Lantern | 25 | Uncommon | held_in_off_hand | unchanged | spell payload ignored and left unchanged |
| 21566 | Rune of Perfection | 25 | Rare |  | skipped | spell payload ignored and left unchanged; visible stat target clamped to +/-20% safety cap |
| 21568 | Rune of Duty | 25 | Rare |  | skipped | spell payload ignored and left unchanged; visible stat target clamped to +/-20% safety cap |
| 43655 | Book of Survival | 25 | Rare | held_in_off_hand | unchanged | spell payload ignored and left unchanged |
| 883922 | Stonemason's Mark | 25 | Rare |  | skipped | spell payload ignored and left unchanged; visible stat target clamped to +/-20% safety cap |
| 4446 | Blackvenom Blade | 26 | Rare | weapon_one_hand | unchanged | spell payload ignored and left unchanged; visible stat target clamped to +/-20% safety cap; rounding remained outside configured tolerance; weapon DPS median from 16 sample(s) within 10 ilvl |
| 7027 | Boots of Darkness | 28 | Uncommon | feet | report-only | spell payload ignored and left unchanged; armor median from 29 sample(s) within 10 ilvl |
| 7046 | Azure Silk Pants | 28 | Uncommon | legs | report-only | spell payload ignored and left unchanged; visible stat target clamped to +/-20% safety cap; armor median from 26 sample(s) within 10 ilvl |
| 13031 | Orb of Mistmantle | 28 | Rare | held_in_off_hand | unchanged | spell payload ignored and left unchanged |
| 3566 | Raptorbane Armor | 29 | Uncommon | chest | report-only | spell payload ignored and left unchanged; visible stat target clamped to +/-20% safety cap; rounding remained outside configured tolerance; final visible stat budget adjusted to stay within +/-20% safety cap; armor median from 31 sample(s) within 10 ilvl |
| 4319 | Azure Silk Gloves | 29 | Uncommon | hands | unchanged | spell payload ignored and left unchanged; visible stat target clamped to +/-20% safety cap; rounding remained outside configured tolerance; final visible stat budget adjusted to stay within +/-20% safety cap; armor median from 27 sample(s) within 10 ilvl |
| 7047 | Hands of Darkness | 29 | Uncommon | hands | report-only | spell payload ignored and left unchanged; visible stat target clamped to +/-20% safety cap; armor median from 27 sample(s) within 10 ilvl |
| 4324 | Azure Silk Vest | 30 | Uncommon | chest | report-only | spell payload ignored and left unchanged; visible stat target clamped to +/-20% safety cap; final visible stat budget adjusted to stay within +/-20% safety cap; armor median from 17 sample(s) within 10 ilvl |
| 6972 | Fire Hardened Hauberk | 30 | Rare | chest | report-only | spell payload ignored and left unchanged; armor median from 13 sample(s) within 10 ilvl |

## Itemsets Intentionally Ignored

| entry | name | item_level | quality_label | slot_key | action | warnings |
| --- | --- | --- | --- | --- | --- | --- |
| 10400 | Blackened Defias Leggings | 18 | Uncommon | legs | unchanged | itemset ignored and left unchanged; armor median from 37 sample(s) within 10 ilvl |
| 10401 | Blackened Defias Gloves | 18 | Uncommon | hands | report-only | itemset ignored and left unchanged; armor median from 25 sample(s) within 10 ilvl |
| 10402 | Blackened Defias Boots | 18 | Uncommon | feet | report-only | itemset ignored and left unchanged; rounding remained outside configured tolerance; armor median from 24 sample(s) within 10 ilvl |
| 10413 | Gloves of the Fang | 19 | Uncommon | hands | report-only | itemset ignored and left unchanged; armor median from 28 sample(s) within 10 ilvl |
| 10412 | Belt of the Fang | 21 | Uncommon | waist | report-only | itemset ignored and left unchanged; armor median from 32 sample(s) within 10 ilvl |
| 10403 | Blackened Defias Belt | 22 | Uncommon | waist | report-only | itemset ignored and left unchanged; armor median from 33 sample(s) within 10 ilvl |
| 6473 | Armor of the Fang | 23 | Uncommon | chest | report-only | itemset ignored and left unchanged; armor median from 39 sample(s) within 10 ilvl |
| 10410 | Leggings of the Fang | 23 | Rare | legs | report-only | itemset ignored and left unchanged; visible stat target clamped to +/-20% safety cap; final visible stat budget adjusted to stay within +/-20% safety cap; armor median from 6 sample(s) within 20 ilvl |
| 10411 | Footpads of the Fang | 23 | Uncommon | feet | unchanged | itemset ignored and left unchanged; armor median from 31 sample(s) within 10 ilvl |
| 10399 | Blackened Defias Armor | 24 | Rare | chest | unchanged | itemset ignored and left unchanged; armor median from 5 sample(s) within 10 ilvl |
| 10332 | Scarlet Boots | 35 | Rare | feet | unchanged | itemset ignored and left unchanged; armor median from 11 sample(s) within 10 ilvl |
| 10333 | Scarlet Wristguards | 36 | Uncommon | wrist | report-only | itemset ignored and left unchanged; armor median from 23 sample(s) within 10 ilvl |
| 10329 | Scarlet Belt | 37 | Uncommon | waist | report-only | itemset ignored and left unchanged; armor median from 16 sample(s) within 10 ilvl |
| 10331 | Scarlet Gauntlets | 38 | Uncommon | hands | report-only | itemset ignored and left unchanged; armor median from 19 sample(s) within 10 ilvl |
| 10328 | Scarlet Chestpiece | 39 | Rare | chest | report-only | itemset ignored and left unchanged; armor median from 13 sample(s) within 10 ilvl |
| 7953 | Mask of Thero-Shan | 42 | Uncommon | head | unchanged | spell payload ignored and left unchanged; itemset ignored and left unchanged |
| 10330 | Scarlet Leggings | 43 | Rare | legs | report-only | itemset ignored and left unchanged; armor median from 10 sample(s) within 10 ilvl |
| 15045 | Green Dragonscale Breastplate | 52 | Rare | chest | report-only | itemset ignored and left unchanged; armor median from 18 sample(s) within 10 ilvl |
| 12424 | Imperial Plate Belt | 53 | Uncommon | waist | report-only | itemset ignored and left unchanged; armor median from 12 sample(s) within 10 ilvl |
| 12428 | Imperial Plate Shoulders | 53 | Uncommon | shoulder | report-only | itemset ignored and left unchanged; armor median from 16 sample(s) within 10 ilvl |
| 12425 | Imperial Plate Bracers | 54 | Uncommon | wrist | report-only | itemset ignored and left unchanged; armor median from 13 sample(s) within 10 ilvl |
| 15046 | Green Dragonscale Leggings | 54 | Rare | legs | report-only | itemset ignored and left unchanged; visible stat target clamped to +/-20% safety cap; armor median from 24 sample(s) within 10 ilvl |
| 15067 | Ironfeather Shoulders | 54 | Rare | shoulder | report-only | itemset ignored and left unchanged; armor median from 22 sample(s) within 10 ilvl |
| 15057 | Stormshroud Pants | 55 | Rare | legs | report-only | itemset ignored and left unchanged; armor median from 30 sample(s) within 10 ilvl |
| 21998 | Gauntlets of Heroism | 55 | Epic | hands | report-only | spell payload ignored and left unchanged; itemset ignored and left unchanged; visible stat target clamped to +/-20% safety cap; final visible stat budget adjusted to stay within +/-20% safety cap; armor median from 16 sample(s) within 20 ilvl |
| 22006 | Darkmantle Gloves | 55 | Epic | hands | report-only | itemset ignored and left unchanged; armor median from 12 sample(s) within 20 ilvl |
| 22015 | Beastmaster's Gloves | 55 | Epic | hands | report-only | spell payload ignored and left unchanged; itemset ignored and left unchanged; armor median from 14 sample(s) within 20 ilvl |
| 22066 | Sorcerer's Gloves | 55 | Epic | hands | report-only | spell payload ignored and left unchanged; itemset ignored and left unchanged; armor median from 4 sample(s) within 10 ilvl |
| 22077 | Deathmist Wraps | 55 | Epic | hands | report-only | spell payload ignored and left unchanged; itemset ignored and left unchanged; visible stat target clamped to +/-20% safety cap; final visible stat budget adjusted to stay within +/-20% safety cap; armor median from 4 sample(s) within 10 ilvl |
| 22081 | Virtuous Gloves | 55 | Epic | hands | report-only | itemset ignored and left unchanged; armor median from 4 sample(s) within 10 ilvl |

## Unknown Stat Types

None.

## Calibration Snapshot

Grouped by expansion, item-level band, quality, slot, and inferred role. Full data is in the calibration CSV.

| count | expansion_band | ilvl_band | quality_label | slot_key | role | median_visible_budget | median_special_weapon_budget | median_total_budget_with_sockets | median_target_budget | median_percent_over_under |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 102 | wrath_t7 | 200-219 | Epic | head | caster | 310.39 | 0.0 | 350.39 | 353.35 | 0.0 |
| 98 | wrath_t7 | 200-219 | Epic | chest | caster | 328.33 | 0.0 | 360.33 | 360.33 | 0.0 |
| 95 | wrath_t9 | 240-259 | Epic | chest | caster | 486.17 | 0.0 | 518.17 | 518.3 | 0.0 |
| 93 | wrath_t7 | 200-219 | Epic | hands | caster | 233.67 | 0.0 | 248.67 | 266.61 | 0.0 |
| 93 | wrath_t7 | 200-219 | Epic | legs | caster | 326.43 | 0.0 | 358.39 | 361.19 | 0.0 |
| 88 | wrath_t7 | 200-219 | Epic | shoulder | caster | 253.03 | 0.0 | 268.735 | 269.03 | 0.0 |
| 88 | wrath_t9 | 240-259 | Epic | legs | caster | 486.17 | 0.0 | 518.17 | 518.43 | 0.0 |
| 84 | wrath_t9 | 240-259 | Epic | head | caster | 463.17 | 0.0 | 503.17 | 496.385 | 0.0 |
| 84 | wrath_t9 | 240-259 | Epic | shoulder | caster | 369.03 | 0.0 | 385.03 | 385.5 | 0.0 |
| 79 | wrath_t8 | 220-239 | Epic | legs | caster | 402.24 | 0.0 | 433.71 | 434.54 | 0.0 |
| 73 | wrath_t8 | 220-239 | Epic | hands | caster | 293.31 | 0.0 | 308.9 | 324.12 | 0.0 |
| 73 | wrath_t9 | 240-259 | Epic | hands | caster | 369.03 | 0.0 | 385.03 | 385.03 | 0.0 |
| 72 | wrath_t8 | 220-239 | Epic | head | caster | 381.875 | 0.0 | 421.06 | 426.24 | 0.0 |
| 71 | classic_endgame | 060-089 | Rare | legs | caster | 56.06 | 0.0 | 56.06 | 48.72 | 0.0 |
| 71 | wrath_t8 | 220-239 | Epic | shoulder | caster | 305.72 | 0.0 | 319.31 | 323.51 | 0.0 |
| 66 | wrath_t8 | 220-239 | Epic | chest | caster | 402.24 | 0.0 | 432.57 | 434.56 | 0.0 |
| 61 | burning_crusade | 105-119 | Rare | chest | caster | 93.02 | 0.0 | 113.35 | 113.35 | 0.0 |
| 61 | burning_crusade | 105-119 | Rare | head | caster | 94.89 | 0.0 | 111.9 | 113.13 | 0.0 |
| 61 | wrath_t7 | 200-219 | Epic | waist | caster | 253.03 | 0.0 | 266.87 | 269.03 | 0.0 |
| 60 | classic_endgame | 060-089 | Epic | shoulder | caster | 60.055 | 0.0 | 60.055 | 61.47 | 0.0 |
| 59 | classic_endgame | 060-089 | Epic | legs | caster | 86.38 | 0.0 | 86.38 | 84.17 | 0.0 |
| 59 | wrath_t7 | 200-219 | Epic | feet | caster | 252.99 | 0.0 | 267.53 | 269.03 | 0.0 |
| 58 | burning_crusade | 105-119 | Rare | legs | caster | 110.37 | 0.0 | 114.615 | 114.99 | 0.0 |
| 57 | classic_endgame | 060-089 | Rare | shoulder | caster | 39.83 | 0.0 | 39.83 | 36.04 | 0.0 |
| 57 | classic_endgame | 060-089 | Epic | feet | caster | 61.47 | 0.0 | 61.47 | 61.26 | 0.0 |
| 56 | classic_endgame | 060-089 | Rare | chest | caster | 51.915 | 0.0 | 51.915 | 48.625 | 0.0 |
| 56 | wrath_t9 | 240-259 | Epic | legs | physical | 465.62 | 0.0 | 497.62 | 499.55 | 0.0 |
| 55 | burning_crusade | 120-134 | Epic | head | caster | 131.0 | 0.0 | 151.3 | 151.0 | 0.0 |
| 55 | classic_endgame | 060-089 | Epic | chest | caster | 72.53 | 0.0 | 72.53 | 70.63 | 0.0 |
| 53 | burning_crusade | 105-119 | Rare | hands | caster | 83.35 | 0.0 | 83.61 | 83.99 | 0.0 |
| 52 | classic_endgame | 060-089 | Epic | head | caster | 84.675 | 0.0 | 84.675 | 84.41 | 0.0 |
| 52 | wrath_t7 | 200-219 | Epic | hands | physical | 237.84 | 0.0 | 250.3 | 269.21 | 0.0 |
| 51 | burning_crusade | 120-134 | Epic | chest | caster | 135.42 | 0.0 | 159.42 | 159.7 | 0.0 |
| 51 | classic_endgame | 060-089 | Epic | wrist | caster | 42.24 | 0.0 | 42.24 | 42.37 | 0.0 |
| 50 | burning_crusade | 105-119 | Rare | shoulder | caster | 74.65 | 0.0 | 89.09 | 89.14 | 0.0 |
| 50 | classic_endgame | 060-089 | Epic | waist | caster | 58.99 | 0.0 | 58.99 | 57.535 | 0.0 |
| 49 | wrath_t7 | 200-219 | Epic | wrist | caster | 200.92 | 0.0 | 202.3 | 204.98 | 0.0 |
| 48 | burning_crusade | 135-149 | Epic | legs | caster | 178.65 | 0.0 | 183.755 | 178.745 | 0.0 |
| 48 | classic_endgame | 060-089 | Rare | head | caster | 53.73 | 0.0 | 53.73 | 50.71 | 0.0 |
| 48 | classic_endgame | 060-089 | Rare | waist | caster | 35.745 | 0.0 | 35.745 | 34.7 | 0.0 |
