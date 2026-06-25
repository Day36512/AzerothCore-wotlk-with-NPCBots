# Item Budget Normalizer Summary

- Generated: 2026-06-22T19:21:37
- Input CSV: `F:\Downloads\First_pass_normalization.csv`
- Config: `C:\AzerothCore\Burning_ Legacy_Server\AzerothCore-wotlk-with-NPCBots\tools\item_budget_normalizer\item_budget_config.yml`
- SQL generation: enabled
- Filters: min ilvl `None`, max ilvl `None`, required level `None`, qualities `any`

## Output Files

- Before/after CSV: `tools\item_budget_normalizer\reports\20260622_192136_normalization_report.csv`
- Calibration CSV: `tools\item_budget_normalizer\reports\20260622_192136_calibration.csv`
- Update SQL: `tools\item_budget_normalizer\reports\20260622_192136_normalization_updates.sql`
- Rollback SQL: `tools\item_budget_normalizer\reports\20260622_192136_normalization_rollback.sql`

## Counts

- Report rows: 18347
- Skipped: 626
- Report-only changes: 0
- Updated SQL rows: 17662
- Unchanged: 59
- Would change if SQL were enabled: 17662

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
| unknown stat type 47 in slot 5 | 4 |
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

## Most Overbudget Before Normalization

| entry | name | item_level | quality_label | slot_key | percent_over_under_before | action |
| --- | --- | --- | --- | --- | --- | --- |
| 22988 | The End of Dreams | 83 | Epic | weapon_main_hand | 594.925 | updated |
| 30313 | Staff of Disintegration | 175 | Legendary | weapon_two_hand | 533.118 | updated |
| 31802 | Fleshling Simulation Staff | 109 | Rare | weapon_one_hand | 500.725 | updated |
| 51453 | Wrathful Gladiator's Gavel | 264 | Epic | weapon_main_hand | 498.062 | updated |
| 51397 | Wrathful Gladiator's Spellblade | 264 | Epic | weapon_main_hand | 490.265 | updated |
| 51406 | Wrathful Gladiator's Blade of Alacrity | 264 | Epic | weapon_main_hand | 490.265 | updated |
| 48519 | Relentless Gladiator's Salvation | 258 | Epic | weapon_main_hand | 483.684 | updated |
| 48408 | Relentless Gladiator's Mageblade | 258 | Epic | weapon_main_hand | 475.33 | updated |
| 49191 | Relentless Gladiator's Blade of Celerity | 258 | Epic | weapon_main_hand | 475.33 | updated |
| 34198 | Stanchion of Primal Instinct | 154 | Epic | weapon_two_hand | 462.843 | updated |
| 35103 | Brutal Gladiator's Staff | 154 | Epic | weapon_two_hand | 461.247 | updated |
| 33716 | Vengeful Gladiator's Staff | 146 | Epic | weapon_two_hand | 453.451 | updated |
| 18747 | Item Properties Test | 63 | Rare | shoulder | 449.117 | updated |
| 42354 | Relentless Gladiator's Gavel | 245 | Epic | weapon_main_hand | 448.274 | updated |
| 33987 | Indalamar's Ring of 200 Spell Crit | 130 | Epic | finger | 446.84 | updated |
| 32014 | Merciless Gladiator's Maul | 136 | Epic | weapon_two_hand | 446.642 | updated |
| 49494 | Honed Fang of the Mystics | 245 | Epic | weapon_main_hand | 444.743 | updated |
| 42348 | Relentless Gladiator's Spellblade | 245 | Epic | weapon_main_hand | 439.778 | updated |
| 49189 | Relentless Gladiator's Blade of Alacrity | 245 | Epic | weapon_main_hand | 439.778 | updated |
| 28476 | Gladiator's Maul | 123 | Epic | weapon_two_hand | 438.633 | updated |

## Most Underbudget Before Normalization

| entry | name | item_level | quality_label | slot_key | percent_over_under_before | action |
| --- | --- | --- | --- | --- | --- | --- |
| 13052 | Warmonger | 52 | Rare | weapon_two_hand | -91.582 | updated |
| 18729 | Screeching Bow | 60 | Rare | ranged | -91.389 | updated |
| 28438 | Dragonmaw | 123 | Epic | weapon_one_hand | -90.16 | updated |
| 7001 | Gravestone Scepter | 29 | Rare | ranged | -90.115 | updated |
| 20082 | Woestave | 52 | Rare | ranged | -89.848 | updated |
| 4446 | Blackvenom Blade | 26 | Rare | weapon_one_hand | -88.792 | updated |
| 3021 | Ranger Bow | 25 | Rare | ranged | -88.275 | updated |
| 18203 | Eskhandar's Right Claw | 66 | Epic | weapon_main_hand | -86.645 | updated |
| 2664 | Spinner Fang | 18 | Uncommon | weapon_one_hand | -86.515 | updated |
| 13349 | Scepter of the Unholy | 63 | Rare | weapon_one_hand | -86.431 | updated |
| 18372 | Blade of the New Moon | 62 | Rare | weapon_one_hand | -86.179 | updated |
| 9602 | Brushwood Blade | 10 | Uncommon | weapon_two_hand | -85.272 | updated |
| 9603 | Gritroot Staff | 10 | Uncommon | weapon_two_hand | -85.272 | updated |
| 11628 | Houndmaster's Bow | 53 | Rare | ranged | -85.176 | updated |
| 11629 | Houndmaster's Rifle | 53 | Rare | ranged | -85.176 | updated |
| 19908 | Sceptre of Smiting | 65 | Rare | weapon_one_hand | -84.37 | updated |
| 19921 | Zulian Hacker | 65 | Rare | weapon_one_hand | -84.37 | updated |
| 1287 | Giant Tarantula Fang | 15 | Uncommon | weapon_one_hand | -83.37 | updated |
| 1394 | Driftwood Club | 15 | Uncommon | weapon_one_hand | -83.37 | updated |
| 2088 | Long Crawler Limb | 15 | Uncommon | weapon_one_hand | -83.37 | updated |

## Weapons Changed

| entry | name | item_level | quality_label | subclass_label | old_dps | new_dps | action | warnings |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 1917 | Jeweled Dagger | 10 | Uncommon | Dagger | 5.312 | 7.812 | updated | rounding remained outside configured tolerance; weapon DPS median from 33 sample(s) within 10 ilvl |
| 9513 | Ley Staff | 10 | Uncommon | Staff | 7.069 | 11.552 | updated | rounding remained outside configured tolerance; weapon DPS median from 18 sample(s) within 10 ilvl |
| 9514 | Arcane Staff | 10 | Uncommon | Staff | 7.069 | 11.552 | updated | rounding remained outside configured tolerance; weapon DPS median from 18 sample(s) within 10 ilvl |
| 9602 | Brushwood Blade | 10 | Uncommon | Two-hand Sword | 7.167 | 11.333 | updated | rounding remained outside configured tolerance; weapon DPS median from 15 sample(s) within 10 ilvl |
| 9603 | Gritroot Staff | 10 | Uncommon | Staff | 6.923 | 11.538 | updated | rounding remained outside configured tolerance; weapon DPS median from 18 sample(s) within 10 ilvl |
| 18957 | Brushwood Blade | 10 | Uncommon | Sword | 5.333 | 7.667 | updated | rounding remained outside configured tolerance; weapon DPS median from 23 sample(s) within 10 ilvl |
| 23423 | Mercenary Greatsword | 10 | Uncommon | Two-hand Sword | 7.069 | 11.379 | updated | rounding remained outside configured tolerance; weapon DPS median from 15 sample(s) within 10 ilvl |
| 23429 | Mercenary Clout | 10 | Uncommon | Two-hand Mace | 7.143 | 9.286 | updated | rounding remained outside configured tolerance; weapon DPS median from 10 sample(s) within 10 ilvl |
| 23430 | Mercenary Sword | 10 | Uncommon | Sword | 5.417 | 7.708 | updated | rounding remained outside configured tolerance; weapon DPS median from 23 sample(s) within 10 ilvl |
| 23431 | Mercenary Stiletto | 10 | Uncommon | Dagger | 5.333 | 7.667 | updated | rounding remained outside configured tolerance; weapon DPS median from 33 sample(s) within 10 ilvl |
| 816 | Small Hand Blade | 11 | Uncommon | Dagger | 6.0 | 7.667 | updated | rounding remained outside configured tolerance; weapon DPS median from 33 sample(s) within 10 ilvl |
| 2281 | Rodentia Flint Axe | 11 | Uncommon | Axe | 6.0 | 7.75 | updated | rounding remained outside configured tolerance; weapon DPS median from 12 sample(s) within 10 ilvl |
| 3223 | Frostmane Scepter | 11 | Uncommon | Mace | 5.909 | 7.955 | updated | rounding remained outside configured tolerance; weapon DPS median from 28 sample(s) within 10 ilvl |
| 4939 | Steady Bastard Sword | 11 | Uncommon | Two-hand Sword | 7.593 | 11.481 | updated | weapon DPS median from 15 sample(s) within 10 ilvl |
| 4947 | Jagged Dagger | 11 | Uncommon | Dagger | 6.0 | 7.667 | updated | rounding remained outside configured tolerance; weapon DPS median from 33 sample(s) within 10 ilvl |
| 4948 | Stinging Mace | 11 | Uncommon | Mace | 5.87 | 7.826 | updated | rounding remained outside configured tolerance; weapon DPS median from 28 sample(s) within 10 ilvl |
| 24339 | Stung | 11 | Uncommon | Sword | 6.0 | 8.2 | updated | rounding remained outside configured tolerance; weapon DPS median from 25 sample(s) within 10 ilvl |
| 26049 | Old Elekk Prod | 11 | Uncommon | Mace | 5.833 | 8.056 | updated | rounding remained outside configured tolerance; weapon DPS median from 28 sample(s) within 10 ilvl |
| 26051 | 2 Stone Sledgehammer | 11 | Uncommon | Two-hand Mace | 7.714 | 9.857 | updated | weapon DPS median from 12 sample(s) within 10 ilvl |
| 26053 | Elekk Handler's Blade | 11 | Uncommon | Sword | 6.0 | 8.25 | updated | rounding remained outside configured tolerance; weapon DPS median from 25 sample(s) within 10 ilvl |
| 27389 | Surplus Bastard Sword | 11 | Uncommon | Two-hand Sword | 7.812 | 11.406 | updated | weapon DPS median from 15 sample(s) within 10 ilvl |
| 28152 | Quel'Thalas Recurve | 11 | Uncommon | Bow | 7.037 | 7.037 | updated | rounding remained outside configured tolerance; weapon damage unchanged: not enough comparable physical weapon samples |
| 33791 | Heavy Copper Longsword | 11 | Uncommon | Sword | 6.0 | 8.2 | updated | rounding remained outside configured tolerance; weapon DPS median from 25 sample(s) within 10 ilvl |
| 2254 | Icepane Warhammer | 12 | Uncommon | Two-hand Mace | 8.485 | 9.697 | updated | rounding remained outside configured tolerance; weapon DPS median from 12 sample(s) within 10 ilvl |
| 3103 | Coldridge Hammer | 12 | Uncommon | Two-hand Mace | 8.333 | 9.815 | updated | rounding remained outside configured tolerance; weapon DPS median from 12 sample(s) within 10 ilvl |
| 4303 | Cranial Thumper | 12 | Uncommon | Mace | 6.429 | 8.036 | updated | rounding remained outside configured tolerance; weapon DPS median from 31 sample(s) within 10 ilvl |
| 4964 | Goblin Smasher | 12 | Uncommon | Two-hand Mace | 8.226 | 9.839 | updated | rounding remained outside configured tolerance; weapon DPS median from 12 sample(s) within 10 ilvl |
| 4971 | Skorn's Hammer | 12 | Uncommon | Mace | 6.481 | 8.148 | updated | rounding remained outside configured tolerance; weapon DPS median from 31 sample(s) within 10 ilvl |
| 4974 | Compact Fighting Knife | 12 | Uncommon | Dagger | 6.333 | 7.667 | updated | rounding remained outside configured tolerance; weapon DPS median from 35 sample(s) within 10 ilvl |
| 27401 | Arugoo's Crossbow of Destruction | 12 | Uncommon | Crossbow | 7.321 | 7.321 | updated | rounding remained outside configured tolerance; weapon damage unchanged: not enough comparable physical weapon samples |

## Armor Changed

| entry | name | item_level | quality_label | subclass_label | old_armor | new_armor | action | warnings |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 16604 | Moon Robes of Elune | 5 | Uncommon | Cloth | 10 | 19 | updated | rounding remained outside configured tolerance; armor median from 17 sample(s) within 10 ilvl |
| 16605 | Friar's Robes of the Light | 5 | Uncommon | Cloth | 10 | 19 | updated | rounding remained outside configured tolerance; armor median from 17 sample(s) within 10 ilvl |
| 16606 | Juju Hex Robes | 5 | Uncommon | Cloth | 10 | 19 | updated | rounding remained outside configured tolerance; armor median from 17 sample(s) within 10 ilvl |
| 16607 | Acolyte's Sacrificial Robes | 5 | Uncommon | Cloth | 10 | 19 | updated | rounding remained outside configured tolerance; armor median from 17 sample(s) within 10 ilvl |
| 23924 | Robes of Silvermoon | 5 | Uncommon | Cloth | 10 | 19 | updated | rounding remained outside configured tolerance; armor median from 17 sample(s) within 10 ilvl |
| 23931 | Azure Watch Robes | 5 | Uncommon | Cloth | 10 | 19 | updated | rounding remained outside configured tolerance; armor median from 17 sample(s) within 10 ilvl |
| 2572 | Red Linen Robe | 10 | Uncommon | Cloth | 19 | 26 | updated | rounding remained outside configured tolerance; armor median from 27 sample(s) within 10 ilvl |
| 2958 | Journeyman's Pants | 10 | Uncommon | Cloth | 17 | 26 | updated | rounding remained outside configured tolerance; armor median from 22 sample(s) within 10 ilvl |
| 2962 | Burnt Leather Breeches | 10 | Uncommon | Leather | 48 | 64 | updated | rounding remained outside configured tolerance; armor median from 27 sample(s) within 10 ilvl |
| 2966 | Warrior's Pants | 10 | Uncommon | Mail | 94 | 132 | updated | rounding remained outside configured tolerance; armor median from 17 sample(s) within 10 ilvl |
| 3000 | Brood Mother Carapace | 10 | Uncommon | Leather | 55 | 70 | updated | rounding remained outside configured tolerance; armor median from 27 sample(s) within 10 ilvl |
| 3471 | Copper Chain Vest | 10 | Uncommon | Mail | 108 | 144 | updated | armor median from 19 sample(s) within 10 ilvl |
| 4861 | Sleek Feathered Tunic | 10 | Uncommon | Leather | 55 | 70 | updated | rounding remained outside configured tolerance; armor median from 27 sample(s) within 10 ilvl |
| 6238 | Brown Linen Robe | 10 | Uncommon | Cloth | 19 | 26 | updated | rounding remained outside configured tolerance; armor median from 27 sample(s) within 10 ilvl |
| 6241 | White Linen Robe | 10 | Uncommon | Cloth | 19 | 26 | updated | rounding remained outside configured tolerance; armor median from 27 sample(s) within 10 ilvl |
| 9598 | Sleeping Robes | 10 | Uncommon | Cloth | 19 | 26 | updated | rounding remained outside configured tolerance; armor median from 27 sample(s) within 10 ilvl |
| 9599 | Barkmail Leggings | 10 | Uncommon | Mail | 94 | 132 | updated | rounding remained outside configured tolerance; armor median from 17 sample(s) within 10 ilvl |
| 17922 | Lionfur Armor | 10 | Uncommon | Leather | 55 | 70 | updated | rounding remained outside configured tolerance; armor median from 27 sample(s) within 10 ilvl |
| 23399 | Fallen Apprentice's Robe | 10 | Uncommon | Cloth | 19 | 26 | updated | rounding remained outside configured tolerance; armor median from 27 sample(s) within 10 ilvl |
| 24107 | Ravager Chitin Tunic | 10 | Uncommon | Mail | 108 | 144 | updated | armor median from 19 sample(s) within 10 ilvl |
| 24108 | Ravager Hide Leggings | 10 | Uncommon | Leather | 48 | 64 | updated | rounding remained outside configured tolerance; armor median from 27 sample(s) within 10 ilvl |
| 24109 | Thick Ravager Belt | 10 | Uncommon | Cloth | 11 | 17 | updated | rounding remained outside configured tolerance; armor median from 14 sample(s) within 10 ilvl |
| 24111 | Kurken Hide Jerkin | 10 | Uncommon | Leather | 55 | 70 | updated | rounding remained outside configured tolerance; armor median from 27 sample(s) within 10 ilvl |
| 24112 | Kurkenstoks | 10 | Uncommon | Cloth | 13 | 21 | updated | rounding remained outside configured tolerance; armor median from 11 sample(s) within 10 ilvl |
| 24113 | Cowlen's Bracers of Kinship | 10 | Uncommon | Cloth | 8 | 14 | updated | rounding remained outside configured tolerance; armor median from 9 sample(s) within 10 ilvl |
| 28146 | Courier's Wraps | 10 | Uncommon | Cloth | 8 | 14 | updated | rounding remained outside configured tolerance; armor median from 9 sample(s) within 10 ilvl |
| 28147 | Tranquillien Scout's Bracers | 10 | Uncommon | Leather | 24 | 33 | updated | rounding remained outside configured tolerance; armor median from 7 sample(s) within 10 ilvl |
| 28148 | Bronze Mail Bracers | 10 | Uncommon | Mail | 47 | 71 | updated | rounding remained outside configured tolerance; armor median from 5 sample(s) within 10 ilvl |
| 2957 | Journeyman's Vest | 11 | Uncommon | Cloth | 21 | 26 | updated | rounding remained outside configured tolerance; armor median from 12 sample(s) within 10 ilvl |
| 2961 | Burnt Leather Vest | 11 | Uncommon | Leather | 58 | 70 | updated | rounding remained outside configured tolerance; armor median from 28 sample(s) within 10 ilvl |

## Items With Sockets

| entry | name | item_level | quality_label | slot_key | socket_budget | action | warnings |
| --- | --- | --- | --- | --- | --- | --- | --- |
| 23229 | Sword of Sockety Goodness | 41 | Uncommon | weapon_one_hand | 16.0 | updated | socket budget counted; socket colors and socket bonus unchanged; target visible budget raised by configured minimum; rounding remained outside configured tolerance |
| 27830 | Circlet of the Victor | 80 | Rare | finger | 8.0 | updated | socket budget counted; socket colors and socket bonus unchanged |
| 27832 | Band of the Victor | 80 | Rare | finger | 8.0 | updated | socket budget counted; socket colors and socket bonus unchanged |
| 27833 | Band of the Victor | 80 | Rare | finger | 8.0 | updated | socket budget counted; socket colors and socket bonus unchanged |
| 27834 | Circlet of the Victor | 80 | Rare | finger | 8.0 | updated | socket budget counted; socket colors and socket bonus unchanged |
| 24021 | Light-Touched Breastplate | 85 | Rare | chest | 24.0 | updated | socket budget counted; socket colors and socket bonus unchanged; rounding remained outside configured tolerance; armor median from 7 sample(s) within 10 ilvl |
| 24022 | Scale Leggings of the Skirmisher | 85 | Rare | legs | 24.0 | updated | socket budget counted; socket colors and socket bonus unchanged; rounding remained outside configured tolerance; armor median from 18 sample(s) within 20 ilvl |
| 24046 | Kilt of Rolling Thunders | 85 | Rare | legs | 24.0 | updated | socket budget counted; socket colors and socket bonus unchanged; armor median from 18 sample(s) within 20 ilvl |
| 24063 | Shifting Sash of Midnight | 85 | Rare | waist | 16.0 | updated | socket budget counted; socket colors and socket bonus unchanged; armor median from 11 sample(s) within 20 ilvl |
| 24064 | Ironsole Clompers | 85 | Rare | feet | 16.0 | updated | socket budget counted; socket colors and socket bonus unchanged; armor median from 13 sample(s) within 20 ilvl |
| 24083 | Lifegiver Britches | 85 | Rare | legs | 24.0 | updated | socket budget counted; socket colors and socket bonus unchanged; rounding remained outside configured tolerance; armor median from 18 sample(s) within 20 ilvl |
| 24090 | Bloodstained Ravager Gauntlets | 85 | Rare | hands | 16.0 | updated | socket budget counted; socket colors and socket bonus unchanged; armor median from 10 sample(s) within 20 ilvl |
| 24091 | Tenacious Defender | 85 | Rare | waist | 16.0 | updated | socket budget counted; socket colors and socket bonus unchanged; armor median from 12 sample(s) within 20 ilvl |
| 153959 | Lightdrinker Dagger | 85 | Rare | weapon_one_hand | 8.0 | updated | socket budget counted; socket colors and socket bonus unchanged; weapon DPS median from 5 sample(s) within 10 ilvl |
| 24387 | Ironblade Gauntlets | 88 | Rare | hands | 16.0 | updated | socket budget counted; socket colors and socket bonus unchanged; armor median from 8 sample(s) within 20 ilvl |
| 24388 | Girdle of the Gale Storm | 88 | Rare | waist | 16.0 | updated | socket budget counted; socket colors and socket bonus unchanged; rounding remained outside configured tolerance; armor median from 8 sample(s) within 20 ilvl |
| 24391 | Kilt of the Night Strider | 88 | Rare | legs | 24.0 | updated | socket budget counted; socket colors and socket bonus unchanged; armor median from 15 sample(s) within 20 ilvl |
| 24393 | Bloody Surgeon's Mitts | 88 | Rare | hands | 16.0 | updated | socket budget counted; socket colors and socket bonus unchanged; armor median from 4 sample(s) within 10 ilvl |
| 24395 | Mindfire Waistband | 88 | Rare | waist | 16.0 | updated | socket budget counted; socket colors and socket bonus unchanged; armor median from 6 sample(s) within 20 ilvl |
| 24396 | Vest of Vengeance | 88 | Rare | chest | 24.0 | updated | socket budget counted; socket colors and socket bonus unchanged; armor median from 5 sample(s) within 10 ilvl |
| 24397 | Raiments of Divine Authority | 88 | Rare | chest | 24.0 | updated | socket budget counted; socket colors and socket bonus unchanged; armor median from 9 sample(s) within 20 ilvl |
| 23362 | Hammer of the Sun | 90 | Epic | weapon_main_hand | 28.0 | updated | socket budget counted; socket colors and socket bonus unchanged; rounding remained outside configured tolerance |
| 23363 | Titanic Breastplate | 90 | Epic | chest | 36.0 | updated | socket budget counted; socket colors and socket bonus unchanged; rounding remained outside configured tolerance |
| 27715 | Circle's Stalwart Helmet | 90 | Uncommon | head | 24.0 | updated | socket budget counted; socket colors and socket bonus unchanged; armor median from 6 sample(s) within 10 ilvl |
| 27717 | Expedition Forager Leggings | 90 | Uncommon | legs | 24.0 | updated | socket budget counted; socket colors and socket bonus unchanged; rounding remained outside configured tolerance; armor median from 6 sample(s) within 10 ilvl |
| 31657 | Chemise of Rebirth | 90 | Uncommon | chest | 24.0 | updated | socket budget counted; socket colors and socket bonus unchanged; armor median from 4 sample(s) within 20 ilvl |
| 31658 | Scout's Hood | 90 | Uncommon | head | 24.0 | updated | socket budget counted; socket colors and socket bonus unchanged; rounding remained outside configured tolerance; armor median from 9 sample(s) within 10 ilvl |
| 24357 | Vest of Living Lightning | 91 | Rare | chest | 24.0 | updated | socket budget counted; socket colors and socket bonus unchanged; armor median from 7 sample(s) within 10 ilvl |
| 24363 | Unscarred Breastplate | 91 | Rare | chest | 24.0 | updated | socket budget counted; socket colors and socket bonus unchanged; armor median from 9 sample(s) within 10 ilvl |
| 24365 | Deft Handguards | 91 | Rare | hands | 16.0 | updated | socket budget counted; socket colors and socket bonus unchanged; armor median from 4 sample(s) within 10 ilvl |

## Spell Payloads Intentionally Ignored

| entry | name | item_level | quality_label | slot_key | action | warnings |
| --- | --- | --- | --- | --- | --- | --- |
| 17 | Martin Fury | 5 | 6 |  | skipped | spell payload ignored and left unchanged |
| 7507 | Arcane Orb | 10 | Uncommon | held_in_off_hand | unchanged | spell payload ignored and left unchanged; rounding remained outside configured tolerance |
| 7508 | Ley Orb | 10 | Uncommon | held_in_off_hand | unchanged | spell payload ignored and left unchanged; rounding remained outside configured tolerance |
| 15866 | Veildust Medicine Bag | 15 | Uncommon | held_in_off_hand | unchanged | spell payload ignored and left unchanged; rounding remained outside configured tolerance |
| 28164 | Tranquillien Flamberge | 15 | Uncommon | weapon_two_hand | updated | spell payload ignored and left unchanged; rounding remained outside configured tolerance; weapon DPS median from 17 sample(s) within 10 ilvl |
| 825438 | Distinct Emerald Pendant | 18 | Rare | neck | updated | spell payload ignored and left unchanged |
| 5183 | Pulsating Hydra Heart | 20 | Rare | held_in_off_hand | updated | spell payload ignored and left unchanged; rounding remained outside configured tolerance |
| 5387 | Enchanted Moonstalker Cloak | 20 | Uncommon | cloak | updated | spell payload ignored and left unchanged; rounding remained outside configured tolerance; armor median from 70 sample(s) within 10 ilvl |
| 22783 | Sunwell Blade | 20 | Uncommon | weapon_one_hand | updated | spell payload ignored and left unchanged; rounding remained outside configured tolerance; weapon DPS median from 34 sample(s) within 10 ilvl |
| 22784 | Sunwell Orb | 20 | Uncommon | held_in_off_hand | unchanged | spell payload ignored and left unchanged; rounding remained outside configured tolerance |
| 1484 | Witching Stave | 22 | Rare | weapon_two_hand | updated | spell payload ignored and left unchanged; weapon DPS median from 14 sample(s) within 10 ilvl |
| 5613 | Staff of the Purifier | 23 | Uncommon | weapon_two_hand | updated | spell payload ignored and left unchanged; weapon DPS median from 25 sample(s) within 10 ilvl |
| 30804 | Bronze Band of Force | 23 | Rare | finger | updated | spell payload ignored and left unchanged |
| 4444 | Black Husk Shield | 24 | Uncommon | shield | updated | spell payload ignored and left unchanged; armor median from 45 sample(s) within 10 ilvl |
| 2950 | Icicle Rod | 25 | Uncommon | weapon_two_hand | updated | spell payload ignored and left unchanged; weapon DPS median from 22 sample(s) within 10 ilvl |
| 4317 | Phoenix Pants | 25 | Uncommon | legs | updated | spell payload ignored and left unchanged; rounding remained outside configured tolerance; armor median from 31 sample(s) within 10 ilvl |
| 5323 | Everglow Lantern | 25 | Uncommon | held_in_off_hand | updated | spell payload ignored and left unchanged |
| 21566 | Rune of Perfection | 25 | Rare |  | skipped | spell payload ignored and left unchanged |
| 21568 | Rune of Duty | 25 | Rare |  | skipped | spell payload ignored and left unchanged |
| 43655 | Book of Survival | 25 | Rare | held_in_off_hand | unchanged | spell payload ignored and left unchanged |
| 883922 | Stonemason's Mark | 25 | Rare |  | skipped | spell payload ignored and left unchanged |
| 4446 | Blackvenom Blade | 26 | Rare | weapon_one_hand | updated | spell payload ignored and left unchanged; weapon DPS median from 16 sample(s) within 10 ilvl |
| 7027 | Boots of Darkness | 28 | Uncommon | feet | updated | spell payload ignored and left unchanged; armor median from 29 sample(s) within 10 ilvl |
| 7046 | Azure Silk Pants | 28 | Uncommon | legs | updated | spell payload ignored and left unchanged; rounding remained outside configured tolerance; armor median from 26 sample(s) within 10 ilvl |
| 13031 | Orb of Mistmantle | 28 | Rare | held_in_off_hand | updated | spell payload ignored and left unchanged |
| 3566 | Raptorbane Armor | 29 | Uncommon | chest | updated | spell payload ignored and left unchanged; armor median from 31 sample(s) within 10 ilvl |
| 4319 | Azure Silk Gloves | 29 | Uncommon | hands | updated | spell payload ignored and left unchanged; rounding remained outside configured tolerance; armor median from 27 sample(s) within 10 ilvl |
| 7047 | Hands of Darkness | 29 | Uncommon | hands | updated | spell payload ignored and left unchanged; rounding remained outside configured tolerance; armor median from 27 sample(s) within 10 ilvl |
| 4324 | Azure Silk Vest | 30 | Uncommon | chest | updated | spell payload ignored and left unchanged; armor median from 17 sample(s) within 10 ilvl |
| 6972 | Fire Hardened Hauberk | 30 | Rare | chest | updated | spell payload ignored and left unchanged; rounding remained outside configured tolerance; armor median from 13 sample(s) within 10 ilvl |

## Itemsets Intentionally Ignored

| entry | name | item_level | quality_label | slot_key | action | warnings |
| --- | --- | --- | --- | --- | --- | --- |
| 10400 | Blackened Defias Leggings | 18 | Uncommon | legs | updated | itemset ignored and left unchanged; armor median from 37 sample(s) within 10 ilvl |
| 10401 | Blackened Defias Gloves | 18 | Uncommon | hands | updated | itemset ignored and left unchanged; armor median from 25 sample(s) within 10 ilvl |
| 10402 | Blackened Defias Boots | 18 | Uncommon | feet | updated | itemset ignored and left unchanged; rounding remained outside configured tolerance; armor median from 24 sample(s) within 10 ilvl |
| 10413 | Gloves of the Fang | 19 | Uncommon | hands | updated | itemset ignored and left unchanged; rounding remained outside configured tolerance; armor median from 28 sample(s) within 10 ilvl |
| 10412 | Belt of the Fang | 21 | Uncommon | waist | updated | itemset ignored and left unchanged; rounding remained outside configured tolerance; armor median from 32 sample(s) within 10 ilvl |
| 10403 | Blackened Defias Belt | 22 | Uncommon | waist | updated | itemset ignored and left unchanged; armor median from 33 sample(s) within 10 ilvl |
| 6473 | Armor of the Fang | 23 | Uncommon | chest | updated | itemset ignored and left unchanged; armor median from 39 sample(s) within 10 ilvl |
| 10410 | Leggings of the Fang | 23 | Rare | legs | updated | itemset ignored and left unchanged; armor median from 6 sample(s) within 20 ilvl |
| 10411 | Footpads of the Fang | 23 | Uncommon | feet | updated | itemset ignored and left unchanged; armor median from 31 sample(s) within 10 ilvl |
| 10399 | Blackened Defias Armor | 24 | Rare | chest | updated | itemset ignored and left unchanged; rounding remained outside configured tolerance; armor median from 5 sample(s) within 10 ilvl |
| 10332 | Scarlet Boots | 35 | Rare | feet | updated | itemset ignored and left unchanged; armor median from 11 sample(s) within 10 ilvl |
| 10333 | Scarlet Wristguards | 36 | Uncommon | wrist | updated | itemset ignored and left unchanged; armor median from 23 sample(s) within 10 ilvl |
| 10329 | Scarlet Belt | 37 | Uncommon | waist | updated | itemset ignored and left unchanged; armor median from 16 sample(s) within 10 ilvl |
| 10331 | Scarlet Gauntlets | 38 | Uncommon | hands | updated | itemset ignored and left unchanged; armor median from 19 sample(s) within 10 ilvl |
| 10328 | Scarlet Chestpiece | 39 | Rare | chest | updated | itemset ignored and left unchanged; armor median from 13 sample(s) within 10 ilvl |
| 7953 | Mask of Thero-Shan | 42 | Uncommon | head | updated | spell payload ignored and left unchanged; itemset ignored and left unchanged |
| 10330 | Scarlet Leggings | 43 | Rare | legs | updated | itemset ignored and left unchanged; armor median from 10 sample(s) within 10 ilvl |
| 15045 | Green Dragonscale Breastplate | 52 | Rare | chest | updated | itemset ignored and left unchanged; armor median from 18 sample(s) within 10 ilvl |
| 12424 | Imperial Plate Belt | 53 | Uncommon | waist | updated | itemset ignored and left unchanged; armor median from 12 sample(s) within 10 ilvl |
| 12428 | Imperial Plate Shoulders | 53 | Uncommon | shoulder | updated | itemset ignored and left unchanged; armor median from 16 sample(s) within 10 ilvl |
| 12425 | Imperial Plate Bracers | 54 | Uncommon | wrist | updated | itemset ignored and left unchanged; rounding remained outside configured tolerance; armor median from 13 sample(s) within 10 ilvl |
| 15046 | Green Dragonscale Leggings | 54 | Rare | legs | updated | itemset ignored and left unchanged; armor median from 24 sample(s) within 10 ilvl |
| 15067 | Ironfeather Shoulders | 54 | Rare | shoulder | updated | itemset ignored and left unchanged; armor median from 22 sample(s) within 10 ilvl |
| 15057 | Stormshroud Pants | 55 | Rare | legs | updated | itemset ignored and left unchanged; armor median from 30 sample(s) within 10 ilvl |
| 21998 | Gauntlets of Heroism | 55 | Epic | hands | updated | spell payload ignored and left unchanged; itemset ignored and left unchanged; armor median from 16 sample(s) within 20 ilvl |
| 22006 | Darkmantle Gloves | 55 | Epic | hands | updated | itemset ignored and left unchanged; armor median from 12 sample(s) within 20 ilvl |
| 22015 | Beastmaster's Gloves | 55 | Epic | hands | updated | spell payload ignored and left unchanged; itemset ignored and left unchanged; armor median from 14 sample(s) within 20 ilvl |
| 22066 | Sorcerer's Gloves | 55 | Epic | hands | updated | spell payload ignored and left unchanged; itemset ignored and left unchanged; armor median from 4 sample(s) within 10 ilvl |
| 22077 | Deathmist Wraps | 55 | Epic | hands | updated | spell payload ignored and left unchanged; itemset ignored and left unchanged; armor median from 4 sample(s) within 10 ilvl |
| 22081 | Virtuous Gloves | 55 | Epic | hands | updated | itemset ignored and left unchanged; armor median from 4 sample(s) within 10 ilvl |

## Unknown Stat Types

| entry | name | item_level | quality_label | unknown_stats | skip_reasons |
| --- | --- | --- | --- | --- | --- |
| 888350 | The 1.5 Ring | 20 | Rare | unknown stat type 1 in slot 4 | unknown stat type 1 in slot 4 |
| 34967 | Juno's Test Gem | 70 | Rare | unknown stat type 39 in slot 2; unknown stat type 40 in slot 3 | unknown stat type 39 in slot 2; unknown stat type 40 in slot 3; duplicate stat type 45 in slot 5; not armor or weapon class; cannot classify inventory slot; name matched exclusion pattern Test  |
| 888467 | Charm of the Skymaster | 77 | Epic | unknown stat type 39 in slot 1 | unknown stat type 39 in slot 1; trinket excluded; cannot classify inventory slot |
| 888468 | Vial of Hakkari Bloodfire | 77 | Epic | unknown stat type 47 in slot 2 | unknown stat type 47 in slot 2; trinket excluded; cannot classify inventory slot |
| 32187 | Chancellor's Heavy Crossbow | 105 | Epic | unknown stat type 39 in slot 4 | unknown stat type 39 in slot 4 |
| 42517 | Savage Gladiator's Piercing Touch | 200 | Rare | unknown stat type 47 in slot 5 | unknown stat type 47 in slot 5 |
| 49312 | Purified Onyxia Blood Talisman | 232 | Epic | unknown stat type 46 in slot 2 | unknown stat type 46 in slot 2; trinket excluded; cannot classify inventory slot |
| 42521 | Relentless Gladiator's Piercing Touch | 245 | Epic | unknown stat type 47 in slot 5 | unknown stat type 47 in slot 5 |
| 49487 | Purified Onyxia Blood Talisman | 245 | Epic | unknown stat type 46 in slot 3 | unknown stat type 46 in slot 3; trinket excluded; cannot classify inventory slot |
| 42539 | Relentless Gladiator's Grimoire | 251 | Epic | unknown stat type 47 in slot 5 | unknown stat type 47 in slot 5 |
| 51531 | Wrathful Gladiator's Piercing Touch | 264 | Epic | unknown stat type 47 in slot 5 | unknown stat type 47 in slot 5 |

## Calibration Snapshot

Grouped by required level, item level, quality, inventory type, subclass, slot, and inferred role. Full data is in the calibration CSV.

| count | required_level | item_level | quality_label | inventory_label | subclass_label | role | median_visible_budget | median_total_budget_with_sockets | median_target_budget | median_percent_over_under |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 25 | 70 | 115 | Rare | Robe | Cloth | caster | 89.35 | 109.77 | 49.323 | 122.552 |
| 23 | 70 | 115 | Rare | Head | Cloth | caster | 87.61 | 106.09 | 49.323 | 115.091 |
| 21 | 70 | 115 | Rare | Shoulder | Cloth | caster | 70.0 | 86.0 | 36.499 | 135.622 |
| 21 | 70 | 115 | Rare | Legs | Cloth | caster | 110.1 | 110.1 | 49.323 | 123.221 |
| 21 | 80 | 232 | Epic | Head | Cloth | caster | 386.24 | 426.24 | 127.133 | 235.271 |
| 21 | 80 | 232 | Epic | Robe | Cloth | caster | 402.56 | 434.24 | 127.133 | 241.564 |
| 20 | 80 | 200 | Epic | Hands | Cloth | caster | 220.63 | 236.63 | 79.316 | 198.337 |
| 20 | 80 | 213 | Epic | Two-hand Weapon | Two-hand Sword | generic | 331.36 | 331.36 | 207.423 | 59.751 |
| 20 | 80 | 232 | Epic | Legs | Cloth | caster | 402.4 | 434.39 | 127.133 | 241.682 |
| 19 | 80 | 232 | Epic | Shoulder | Cloth | caster | 306.9 | 322.9 | 94.078 | 243.224 |
| 18 | 80 | 200 | Epic | Robe | Cloth | caster | 292.19 | 323.16 | 107.184 | 201.499 |
| 18 | 80 | 245 | Epic | Robe | Cloth | caster | 456.41 | 488.41 | 135.359 | 260.825 |
| 17 | 70 | 115 | Rare | Hands | Cloth | caster | 73.33 | 76.13 | 36.499 | 108.58 |
| 17 | 70 | 115 | Rare | Finger | Misc | caster | 51.05 | 51.05 | 27.621 | 84.823 |
| 17 | 80 | 200 | Epic | Legs | Cloth | caster | 291.16 | 323.16 | 107.184 | 201.499 |
| 17 | 80 | 226 | Epic | Finger | Misc | caster | 226.69 | 229.82 | 69.081 | 232.681 |
| 17 | 80 | 232 | Epic | Hands | Cloth | caster | 308.12 | 324.12 | 94.078 | 244.521 |
| 17 | 80 | 232 | Epic | Hands | Plate | generic | 274.34 | 290.34 | 94.078 | 208.615 |
| 16 | 80 | 200 | Epic | Head | Cloth | caster | 276.28 | 315.72 | 107.184 | 194.558 |
| 16 | 80 | 200 | Epic | Finger | Misc | caster | 179.015 | 179.015 | 60.023 | 198.243 |
| 16 | 80 | 213 | Epic | Robe | Cloth | caster | 328.43 | 360.36 | 115.235 | 212.718 |
| 16 | 80 | 232 | Epic | Head | Plate | tank | 380.055 | 420.055 | 127.133 | 230.406 |
| 16 | 80 | 245 | Epic | Head | Cloth | caster | 440.67 | 480.67 | 135.359 | 255.107 |
| 15 | 60 | 75 | Epic | Two-hand Weapon | Two-hand Axe | physical | 33.39 | 33.39 | 62.451 | -46.534 |
| 15 | 80 | 200 | Epic | Neck | Misc | caster | 179.93 | 179.93 | 60.023 | 199.767 |
| 15 | 80 | 200 | Epic | Shoulder | Cloth | caster | 222.4 | 238.4 | 79.316 | 200.568 |
| 15 | 80 | 226 | Epic | Neck | Misc | caster | 227.46 | 229.85 | 69.081 | 232.724 |
| 15 | 80 | 232 | Epic | Shoulder | Plate | generic | 282.11 | 298.11 | 94.078 | 216.874 |
| 15 | 80 | 232 | Epic | Shoulder | Plate | tank | 306.74 | 322.74 | 94.078 | 243.054 |
| 15 | 80 | 232 | Epic | Chest | Plate | tank | 370.19 | 402.19 | 127.133 | 216.354 |
| 14 | 80 | 226 | Epic | Finger | Misc | physical | 210.085 | 217.89 | 69.081 | 215.411 |
| 14 | 80 | 232 | Epic | Head | Plate | generic | 350.755 | 390.755 | 127.133 | 207.359 |
| 14 | 80 | 245 | Epic | Legs | Cloth | caster | 456.41 | 488.41 | 135.359 | 260.825 |
| 13 | 58 | 63 | Rare | Robe | Cloth | caster | 47.16 | 47.16 | 24.688 | 91.022 |
| 13 | 70 | 115 | Rare | Chest | Plate | generic | 94.14 | 118.14 | 49.323 | 139.522 |
| 13 | 80 | 200 | Epic | Shield | Shield | tank | 175.37 | 175.37 | 79.316 | 121.102 |
| 13 | 80 | 213 | Epic | Head | Cloth | caster | 313.19 | 353.19 | 115.235 | 206.496 |
| 13 | 80 | 232 | Epic | Head | Leather | physical | 385.46 | 425.46 | 127.133 | 234.657 |
| 13 | 80 | 232 | Epic | Head | Mail | caster | 386.24 | 426.24 | 127.133 | 235.271 |
| 13 | 80 | 245 | Epic | Wrist | Cloth | caster | 269.4 | 272.16 | 75.801 | 259.044 |
