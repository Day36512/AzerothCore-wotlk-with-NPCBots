# Item Budget Normalizer Summary

- Generated: 2026-06-22T20:29:09
- Input CSV: `F:\Downloads\First_pass_normalization.csv`
- Config: `C:\AzerothCore\Burning_ Legacy_Server\AzerothCore-wotlk-with-NPCBots\tools\item_budget_normalizer\item_budget_config.yml`
- SQL generation: enabled
- Target mode: `peer`
- Peer minimum samples: `8`; outlier tolerance: `+/-15.0%`
- Visible budget per-pass cap: `+/-20.0%`; allow large changes: `False`
- Calibration sanity: not sane; worst grouped median `290.713%`
- Filters: min ilvl `None`, max ilvl `None`, required level `None`, qualities `any`

## Output Files

- Before/after CSV: `tools\item_budget_normalizer\reports\20260622_202908_normalization_report.csv`
- Calibration CSV: `tools\item_budget_normalizer\reports\20260622_202908_calibration.csv`
- Update SQL: not generated
- Rollback SQL: not generated

## Counts

- Report rows: 18347
- Skipped: 620
- Report-only changes: 2212
- Updated SQL rows: 0
- Unchanged: 15515
- Would change if SQL were enabled: 2212
- Special weapon-stat rows: 1670
- Peer outliers: 2629
- SQL-blocked rows: 13779

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

| entry | name | item_level | quality_label | slot_key | role | target_source | target_sample_count | percent_over_under_before | action |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 27892 | Cloak of the Inciter | 112 | Rare | cloak | generic | peer_exact_ilvl_low_sample | 1 | 634.453 | unchanged |
| 31802 | Fleshling Simulation Staff | 109 | Rare | weapon_one_hand | physical | peer_exact_ilvl_low_sample | 3 | 444.944 | unchanged |
| 30313 | Staff of Disintegration | 175 | Legendary | weapon_two_hand | physical | peer_exact_ilvl_low_sample | 1 | 353.616 | unchanged |
| 37739 | Brutal Gladiator's Blade of Alacrity | 154 | Epic | weapon_main_hand | generic | peer_exact_ilvl_low_sample | 3 | 331.595 | report-only |
| 51876 | Abomination Knuckles | 264 | Epic | weapon_main_hand | physical | peer_exact_ilvl_low_sample | 1 | 322.907 | unchanged |
| 32955 | Tom's Boots 2 | 130 | Epic | feet | generic | peer_exact_ilvl_low_sample | 1 | 322.276 | unchanged |
| 37740 | Brutal Gladiator's Swift Judgement | 154 | Epic | weapon_main_hand | generic | peer_exact_ilvl_low_sample | 3 | 321.732 | report-only |
| 32958 | Tom's Bracer 2 | 130 | Epic | wrist | generic | peer_exact_ilvl_low_sample | 1 | 316.866 | unchanged |
| 32418 | Tom's Legs 2 | 130 | Epic | legs | generic | peer_exact_ilvl_low_sample | 1 | 315.229 | unchanged |
| 32954 | Tom's Boots 1 | 115 | Epic | feet | generic | peer_exact_ilvl_low_sample | 1 | 301.368 | unchanged |
| 38206 | Wand of Blinding Light | 146 | Uncommon | ranged | caster | peer_exact_ilvl_low_sample | 1 | 300.0 | unchanged |
| 23044 | Harbinger of Doom | 83 | Epic | weapon_one_hand | generic | peer_+/-10_ilvl | 11 | 298.294 | report-only |
| 24106 | Thick Felsteel Necklace | 113 | Rare | neck | generic | peer_+/-6_ilvl | 9 | 290.713 | report-only |
| 13139 | Guttbuster | 50 | Rare | ranged | physical | peer_+/-5_ilvl | 8 | 266.667 | report-only |
| 28597 | Panzar'Thar Breastplate | 115 | Epic | chest | tank | peer_exact_ilvl_low_sample | 1 | 259.841 | unchanged |
| 24116 | Eye of the Night | 114 | Rare | neck | generic | peer_exact_ilvl_low_sample | 5 | 248.259 | unchanged |
| 23540 | Felsteel Longblade | 105 | Epic | weapon_one_hand | physical | peer_exact_ilvl_low_sample | 3 | 236.287 | unchanged |
| 21242 | Blessed Qiraji War Axe | 79 | Epic | weapon_one_hand | generic | peer_+/-10_ilvl | 15 | 220.149 | report-only |
| 22808 | The Castigator | 83 | Epic | weapon_one_hand | generic | peer_+/-10_ilvl | 11 | 220.149 | report-only |
| 21244 | Blessed Qiraji Pugio | 79 | Epic | weapon_one_hand | generic | peer_+/-10_ilvl | 15 | 205.864 | report-only |

## Most Underbudget Before Normalization

| entry | name | item_level | quality_label | slot_key | role | target_source | target_sample_count | percent_over_under_before | action |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 65492 | Broadsword of the Crown | 18 | Rare | weapon_one_hand | physical | peer_+/-5_ilvl | 8 | -100.0 | report-only |
| 30504 | Leafblade Dagger | 19 | Uncommon | weapon_one_hand | physical | peer_+/-5_ilvl | 52 | -100.0 | unchanged |
| 5191 | Cruel Barb | 24 | Rare | weapon_one_hand | physical | peer_+/-5_ilvl | 23 | -100.0 | unchanged |
| 4825 | Callous Axe | 29 | Uncommon | weapon_one_hand | physical | peer_+/-5_ilvl | 17 | -100.0 | unchanged |
| 16886 | Outlaw Sabre | 30 | Rare | weapon_one_hand | physical | peer_+/-5_ilvl | 24 | -100.0 | unchanged |
| 12259 | Glinting Steel Dagger | 36 | Uncommon | weapon_one_hand | physical | peer_+/-5_ilvl | 15 | -100.0 | unchanged |
| 4983 | Rock Pulverizer | 42 | Uncommon | weapon_two_hand | physical | peer_+/-5_ilvl | 9 | -100.0 | unchanged |
| 13138 | The Silencer | 42 | Rare | ranged | physical | peer_+/-10_ilvl | 8 | -100.0 | unchanged |
| 10823 | Vanquisher's Sword | 44 | Rare | weapon_one_hand | physical | peer_+/-5_ilvl | 17 | -100.0 | unchanged |
| 44218 | Darkmoon Executioner | 45 | Rare | weapon_two_hand | physical | peer_+/-5_ilvl | 14 | -100.0 | unchanged |
| 30073 | Light Emberforged Hammer | 52 | Rare | weapon_one_hand | physical | peer_+/-5_ilvl | 17 | -100.0 | unchanged |
| 12527 | Ribsplitter | 53 | Rare | weapon_one_hand | physical | peer_+/-5_ilvl | 16 | -100.0 | unchanged |
| 17717 | Megashot Rifle | 53 | Rare | ranged | physical | peer_+/-5_ilvl | 9 | -100.0 | unchanged |
| 15862 | Blitzcleaver | 54 | Uncommon | weapon_one_hand | physical | peer_+/-5_ilvl | 8 | -100.0 | unchanged |
| 11923 | The Hammer of Grace | 57 | Rare | weapon_main_hand | caster | peer_+/-10_ilvl | 11 | -100.0 | unchanged |
| 12653 | Riphook | 59 | Rare | ranged | physical | peer_+/-5_ilvl | 14 | -100.0 | unchanged |
| 12061 | Blade of Reckoning | 60 | Uncommon | weapon_one_hand | physical | peer_+/-5_ilvl | 10 | -100.0 | unchanged |
| 18484 | Cho'Rush's Blade | 61 | Rare | weapon_one_hand | physical | peer_+/-5_ilvl | 34 | -100.0 | unchanged |
| 13368 | Bonescraper | 62 | Rare | weapon_one_hand | physical | peer_+/-5_ilvl | 32 | -100.0 | unchanged |
| 20675 | Soulrender | 62 | Rare | weapon_one_hand | physical | peer_+/-5_ilvl | 32 | -100.0 | unchanged |

## Calibration Sanity Offenders

| expansion_band | ilvl_band | item_level | quality_label | slot_key | role | peer_group | count | median_percent_over_under | median_target_budget |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| burning_crusade | 105-119 | 113 | Rare | neck | generic | quality=3;slot=neck;armor=0;weapon=none;role=generic | 1 | 290.713 | 12.06 |
| burning_crusade | 105-119 | 112 | Rare | cloak | generic | quality=3;slot=cloak;armor=1;weapon=none;role=generic | 2 | 274.034 | 33.545 |
| classic_leveling | 050-059 | 50 | Rare | ranged | physical | quality=3;slot=ranged;armor=none;weapon=ranged:bow_gun_crossbow;role=physical | 1 | 266.667 | 3.0 |
| classic_endgame | 060-089 | 83 | Epic | weapon_one_hand | generic | quality=4;slot=weapon_one_hand;armor=none;weapon=weapon_one_hand:physical;role=generic | 2 | 259.222 | 9.38 |
| classic_endgame | 060-089 | 79 | Epic | weapon_one_hand | generic | quality=4;slot=weapon_one_hand;armor=none;weapon=weapon_one_hand:physical;role=generic | 2 | 213.006 | 9.38 |
| classic_leveling | 020-029 | 20 | Uncommon | weapon_one_hand | generic | quality=2;slot=weapon_one_hand;armor=none;weapon=weapon_one_hand:physical;role=generic | 4 | 200.0 | 0.67 |
| burning_crusade | 090-104 | 103 | Rare | neck | generic | quality=3;slot=neck;armor=0;weapon=none;role=generic | 1 | 182.579 | 20.55 |
| classic_endgame | 060-089 | 79 | Epic | weapon_two_hand | physical | quality=4;slot=weapon_two_hand;armor=none;weapon=weapon_two_hand:physical;role=physical | 1 | 170.71 | 33.39 |
| classic_endgame | 060-089 | 72 | Epic | ranged | physical | quality=4;slot=ranged;armor=none;weapon=ranged:bow_gun_crossbow;role=physical | 1 | 153.5 | 10.0 |
| wrath_pre_raid | 165-199 | 175 | Legendary | weapon_two_hand | physical | quality=5;slot=weapon_two_hand;armor=none;weapon=weapon_two_hand:physical;role=physical | 2 | 137.831 | 277.5 |
| burning_crusade | 120-134 | 130 | Epic | finger | generic | quality=4;slot=finger;armor=0;weapon=none;role=generic | 1 | 134.316 | 85.355 |
| classic_endgame | 060-089 | 66 | Rare | weapon_main_hand | caster | quality=3;slot=weapon_main_hand;armor=none;weapon=weapon_main_hand:caster;role=caster | 1 | 130.727 | 5.5 |
| classic_endgame | 060-089 | 81 | Epic | weapon_one_hand | generic | quality=4;slot=weapon_one_hand;armor=none;weapon=weapon_one_hand:physical;role=generic | 1 | 128.038 | 9.38 |
| wrath_t10 | 260-284 | 264 | Epic | weapon_main_hand | physical | quality=4;slot=weapon_main_hand;armor=none;weapon=weapon_main_hand:physical;role=physical | 2 | 123.277 | 120.87 |
| burning_crusade | 120-134 | 130 | Epic | feet | generic | quality=4;slot=feet;armor=4;weapon=none;role=generic | 2 | 122.979 | 69.985 |
| burning_crusade | 150-164 | 154 | Epic | weapon_main_hand | generic | quality=4;slot=weapon_main_hand;armor=none;weapon=weapon_main_hand:physical;role=generic | 4 | 122.722 | 184.615 |
| classic_endgame | 060-089 | 81 | Epic | ranged | physical | quality=4;slot=ranged;armor=none;weapon=ranged:bow_gun_crossbow;role=physical | 2 | 120.682 | 4.69 |
| burning_crusade | 120-134 | 130 | Epic | wrist | generic | quality=4;slot=wrist;armor=4;weapon=none;role=generic | 2 | 120.427 | 51.945 |
| burning_crusade | 120-134 | 130 | Epic | legs | generic | quality=4;slot=legs;armor=4;weapon=none;role=generic | 2 | 119.656 | 93.205 |
| classic_leveling | 040-049 | 44 | Uncommon | waist | caster | quality=2;slot=waist;armor=1;weapon=none;role=caster | 1 | 115.45 | 13.01 |
| burning_crusade | 105-119 | 115 | Epic | feet | generic | quality=4;slot=feet;armor=4;weapon=none;role=generic | 2 | 113.141 | 60.465 |
| burning_crusade | 135-149 | 146 | Uncommon | ranged | caster | quality=2;slot=ranged;armor=none;weapon=ranged:caster;role=caster | 2 | 112.5 | 20.0 |
| classic_endgame | 060-089 | 70 | Epic | weapon_two_hand | physical | quality=4;slot=weapon_two_hand;armor=none;weapon=weapon_two_hand:physical;role=physical | 1 | 102.845 | 33.39 |
| classic_endgame | 060-089 | 78 | Epic | weapon_two_hand | physical | quality=4;slot=weapon_two_hand;armor=none;weapon=weapon_two_hand:physical;role=physical | 8 | 102.066 | 33.39 |
| classic_endgame | 060-089 | 71 | Rare | hands | caster | quality=3;slot=hands;armor=1;weapon=none;role=caster | 1 | 100.708 | 25.43 |
| burning_crusade | 090-104 | 96 | Uncommon | weapon_two_hand | physical | quality=2;slot=weapon_two_hand;armor=none;weapon=weapon_two_hand:physical;role=physical | 1 | -100.0 | 47.0 |
| burning_crusade | 090-104 | 100 | Rare | ranged | physical | quality=3;slot=ranged;armor=none;weapon=ranged:bow_gun_crossbow;role=physical | 1 | -100.0 | 12.0 |
| burning_crusade | 090-104 | 102 | Uncommon | weapon_main_hand | caster | quality=2;slot=weapon_main_hand;armor=none;weapon=weapon_main_hand:caster;role=caster | 3 | -100.0 | 13.0 |
| burning_crusade | 105-119 | 105 | Uncommon | weapon_main_hand | caster | quality=2;slot=weapon_main_hand;armor=none;weapon=weapon_main_hand:caster;role=caster | 3 | -100.0 | 15.525 |
| burning_crusade | 105-119 | 108 | Uncommon | weapon_main_hand | caster | quality=2;slot=weapon_main_hand;armor=none;weapon=weapon_main_hand:caster;role=caster | 3 | -100.0 | 10.86 |
| burning_crusade | 120-134 | 120 | Epic | weapon_one_hand | physical | quality=4;slot=weapon_one_hand;armor=none;weapon=weapon_one_hand:physical;role=physical | 1 | -100.0 | 32.0 |
| burning_crusade | 120-134 | 130 | Epic | weapon_two_hand | physical | quality=4;slot=weapon_two_hand;armor=none;weapon=weapon_two_hand:physical;role=physical | 1 | -100.0 | 110.435 |
| burning_crusade | 150-164 | 151 | Epic | weapon_one_hand | physical | quality=4;slot=weapon_one_hand;armor=none;weapon=weapon_one_hand:physical;role=physical | 1 | -100.0 | 42.77 |
| classic_endgame | 060-089 | 68 | Rare | weapon_one_hand | physical | quality=3;slot=weapon_one_hand;armor=none;weapon=weapon_one_hand:physical;role=physical | 1 | -100.0 | 15.0 |
| classic_endgame | 060-089 | 68 | Epic | ranged | physical | quality=4;slot=ranged;armor=none;weapon=ranged:bow_gun_crossbow;role=physical | 2 | -100.0 | 10.0 |
| classic_endgame | 060-089 | 76 | Epic | weapon_two_hand | physical | quality=4;slot=weapon_two_hand;armor=none;weapon=weapon_two_hand:physical;role=physical | 1 | -100.0 | 33.39 |
| classic_leveling | 010-019 | 16 | Uncommon | weapon_one_hand | generic | quality=2;slot=weapon_one_hand;armor=none;weapon=weapon_one_hand:physical;role=generic | 2 | 100.0 | 0.67 |
| classic_leveling | 010-019 | 17 | Uncommon | weapon_one_hand | generic | quality=2;slot=weapon_one_hand;armor=none;weapon=weapon_one_hand:physical;role=generic | 1 | 100.0 | 0.67 |
| classic_leveling | 010-019 | 18 | Rare | weapon_one_hand | physical | quality=3;slot=weapon_one_hand;armor=none;weapon=weapon_one_hand:physical;role=physical | 1 | -100.0 | 5.34 |
| classic_leveling | 020-029 | 20 | Uncommon | waist | healer | quality=2;slot=waist;armor=1;weapon=none;role=healer | 1 | 100.0 | 4.0 |

## SQL Blocked Rows

| entry | name | item_level | quality_label | slot_key | role | target_source | target_sample_count | percent_over_under_before | warnings |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 16604 | Moon Robes of Elune | 5 | Uncommon | chest | healer | peer_+/-10_ilvl | 15 | -50.0 | SQL blocked: peer target requires 100.0% visible budget change; visible stat target clamped to +/-20% safety cap; rounding remained outside configured tolerance; armor median from 10 sample(s) within 5 ilvl |
| 16605 | Friar's Robes of the Light | 5 | Uncommon | chest | healer | peer_+/-10_ilvl | 15 | -50.0 | SQL blocked: peer target requires 100.0% visible budget change; visible stat target clamped to +/-20% safety cap; rounding remained outside configured tolerance; armor median from 10 sample(s) within 5 ilvl |
| 16606 | Juju Hex Robes | 5 | Uncommon | chest | healer | peer_+/-10_ilvl | 15 | -50.0 | SQL blocked: peer target requires 100.0% visible budget change; visible stat target clamped to +/-20% safety cap; rounding remained outside configured tolerance; armor median from 10 sample(s) within 5 ilvl |
| 16607 | Acolyte's Sacrificial Robes | 5 | Uncommon | chest | healer | peer_+/-10_ilvl | 15 | -50.0 | SQL blocked: peer target requires 100.0% visible budget change; visible stat target clamped to +/-20% safety cap; rounding remained outside configured tolerance; armor median from 10 sample(s) within 5 ilvl |
| 23924 | Robes of Silvermoon | 5 | Uncommon | chest | healer | peer_+/-10_ilvl | 15 | -50.0 | SQL blocked: peer target requires 100.0% visible budget change; visible stat target clamped to +/-20% safety cap; rounding remained outside configured tolerance; armor median from 10 sample(s) within 5 ilvl |
| 23931 | Azure Watch Robes | 5 | Uncommon | chest | healer | peer_+/-10_ilvl | 15 | -50.0 | SQL blocked: peer target requires 100.0% visible budget change; visible stat target clamped to +/-20% safety cap; rounding remained outside configured tolerance; armor median from 10 sample(s) within 5 ilvl |
| 2572 | Red Linen Robe | 10 | Uncommon | chest | caster | peer_+/-10_ilvl | 15 | -76.959 | SQL blocked: peer target requires 334.0% visible budget change; visible stat target clamped to +/-20% safety cap; rounding remained outside configured tolerance; armor unchanged: within +/-15% peer tolerance from 17 sample(s) |
| 2958 | Journeyman's Pants | 10 | Uncommon | legs | healer | peer_+/-10_ilvl | 15 | -83.333 | SQL blocked: peer target requires 500.0% visible budget change; visible stat target clamped to +/-20% safety cap; rounding remained outside configured tolerance; armor median from 22 sample(s) within 10 ilvl |
| 2962 | Burnt Leather Breeches | 10 | Uncommon | legs | physical | peer_+/-5_ilvl | 9 | -50.0 | SQL blocked: peer target requires 100.0% visible budget change; visible stat target clamped to +/-20% safety cap; rounding remained outside configured tolerance; armor unchanged: within +/-15% peer tolerance from 12 sample(s) |
| 2966 | Warrior's Pants | 10 | Uncommon | legs | generic | no_peer_skip | 0 | 0.0 | normal stats unchanged: not enough comparable peer samples; armor unchanged: within +/-15% peer tolerance from 8 sample(s) |
| 3000 | Brood Mother Carapace | 10 | Uncommon | chest | generic | no_peer_skip | 0 | 0.0 | normal stats unchanged: not enough comparable peer samples; armor unchanged: within +/-15% peer tolerance from 15 sample(s) |
| 3471 | Copper Chain Vest | 10 | Uncommon | chest | physical | peer_+/-10_ilvl | 13 | -75.062 | SQL blocked: peer target requires 301.0% visible budget change; visible stat target clamped to +/-20% safety cap; rounding remained outside configured tolerance; armor median from 11 sample(s) within 5 ilvl |
| 4861 | Sleek Feathered Tunic | 10 | Uncommon | chest | physical | peer_+/-5_ilvl | 9 | -57.265 | SQL blocked: peer target requires 134.0% visible budget change; visible stat target clamped to +/-20% safety cap; rounding remained outside configured tolerance; armor unchanged: within +/-15% peer tolerance from 15 sample(s) |
| 6238 | Brown Linen Robe | 10 | Uncommon | chest | healer | peer_+/-5_ilvl | 15 | -50.0 | SQL blocked: peer target requires 100.0% visible budget change; visible stat target clamped to +/-20% safety cap; rounding remained outside configured tolerance; armor unchanged: within +/-15% peer tolerance from 17 sample(s) |
| 6241 | White Linen Robe | 10 | Uncommon | chest | caster | peer_+/-10_ilvl | 15 | -76.959 | SQL blocked: peer target requires 334.0% visible budget change; visible stat target clamped to +/-20% safety cap; rounding remained outside configured tolerance; armor unchanged: within +/-15% peer tolerance from 17 sample(s) |
| 7507 | Arcane Orb | 10 | Uncommon | held_in_off_hand | healer | no_peer_skip | 0 | 0.0 | spell payload ignored and left unchanged; normal stats unchanged: not enough comparable peer samples |
| 7508 | Ley Orb | 10 | Uncommon | held_in_off_hand | healer | no_peer_skip | 0 | 0.0 | spell payload ignored and left unchanged; normal stats unchanged: not enough comparable peer samples |
| 9513 | Ley Staff | 10 | Uncommon | weapon_two_hand | healer | peer_+/-10_ilvl | 13 | -66.667 | SQL blocked: peer target requires 200.0% visible budget change; visible stat target clamped to +/-20% safety cap; rounding remained outside configured tolerance; weapon DPS median from 17 sample(s) within 10 ilvl |
| 9514 | Arcane Staff | 10 | Uncommon | weapon_two_hand | healer | peer_+/-10_ilvl | 13 | -66.667 | SQL blocked: peer target requires 200.0% visible budget change; visible stat target clamped to +/-20% safety cap; rounding remained outside configured tolerance; weapon DPS median from 17 sample(s) within 10 ilvl |
| 9598 | Sleeping Robes | 10 | Uncommon | chest | generic | no_peer_skip | 0 | 0.0 | normal stats unchanged: not enough comparable peer samples; armor unchanged: within +/-15% peer tolerance from 17 sample(s) |
| 9599 | Barkmail Leggings | 10 | Uncommon | legs | generic | no_peer_skip | 0 | 0.0 | normal stats unchanged: not enough comparable peer samples; armor unchanged: within +/-15% peer tolerance from 8 sample(s) |
| 9602 | Brushwood Blade | 10 | Uncommon | weapon_two_hand | generic | peer_+/-10_ilvl | 12 | -77.778 | SQL blocked: peer target requires 350.0% visible budget change; visible stat target clamped to +/-20% safety cap; rounding remained outside configured tolerance; weapon DPS median from 15 sample(s) within 10 ilvl |
| 9603 | Gritroot Staff | 10 | Uncommon | weapon_two_hand | generic | peer_+/-10_ilvl | 12 | -77.778 | SQL blocked: peer target requires 350.0% visible budget change; visible stat target clamped to +/-20% safety cap; rounding remained outside configured tolerance; weapon DPS median from 17 sample(s) within 10 ilvl |
| 15925 | Journeyman's Stave | 10 | Uncommon | held_in_off_hand | generic | no_peer_skip | 0 | 0.0 | normal stats unchanged: not enough comparable peer samples |
| 17922 | Lionfur Armor | 10 | Uncommon | chest | caster | no_peer_skip | 0 | 0.0 | normal stats unchanged: not enough comparable peer samples; armor unchanged: within +/-15% peer tolerance from 15 sample(s) |
| 23399 | Fallen Apprentice's Robe | 10 | Uncommon | chest | caster | peer_+/-10_ilvl | 15 | -61.521 | SQL blocked: peer target requires 159.9% visible budget change; visible stat target clamped to +/-20% safety cap; rounding remained outside configured tolerance; armor unchanged: within +/-15% peer tolerance from 17 sample(s) |
| 23423 | Mercenary Greatsword | 10 | Uncommon | weapon_two_hand | physical | peer_+/-5_ilvl | 11 | -50.0 | SQL blocked: peer target requires 100.0% visible budget change; visible stat target clamped to +/-20% safety cap; rounding remained outside configured tolerance; weapon DPS median from 15 sample(s) within 10 ilvl |
| 23429 | Mercenary Clout | 10 | Uncommon | weapon_two_hand | physical | peer_+/-5_ilvl | 11 | -50.0 | SQL blocked: peer target requires 100.0% visible budget change; visible stat target clamped to +/-20% safety cap; rounding remained outside configured tolerance; weapon DPS median from 10 sample(s) within 10 ilvl |
| 24107 | Ravager Chitin Tunic | 10 | Uncommon | chest | physical | peer_+/-10_ilvl | 13 | -75.062 | SQL blocked: peer target requires 301.0% visible budget change; visible stat target clamped to +/-20% safety cap; rounding remained outside configured tolerance; armor median from 11 sample(s) within 5 ilvl |
| 24108 | Ravager Hide Leggings | 10 | Uncommon | legs | physical | peer_+/-5_ilvl | 9 | -50.0 | SQL blocked: peer target requires 100.0% visible budget change; visible stat target clamped to +/-20% safety cap; rounding remained outside configured tolerance; armor unchanged: within +/-15% peer tolerance from 12 sample(s) |
| 24109 | Thick Ravager Belt | 10 | Uncommon | waist | caster | peer_+/-10_ilvl | 8 | -76.019 | SQL blocked: peer target requires 317.0% visible budget change; visible stat target clamped to +/-20% safety cap; rounding remained outside configured tolerance; armor median from 14 sample(s) within 10 ilvl |
| 24111 | Kurken Hide Jerkin | 10 | Uncommon | chest | physical | peer_+/-5_ilvl | 9 | -57.265 | SQL blocked: peer target requires 134.0% visible budget change; visible stat target clamped to +/-20% safety cap; rounding remained outside configured tolerance; armor unchanged: within +/-15% peer tolerance from 15 sample(s) |
| 24112 | Kurkenstoks | 10 | Uncommon | feet | generic | no_peer_skip | 0 | 0.0 | normal stats unchanged: not enough comparable peer samples; armor median from 11 sample(s) within 10 ilvl |
| 24113 | Cowlen's Bracers of Kinship | 10 | Uncommon | wrist | generic | no_peer_skip | 0 | 0.0 | normal stats unchanged: not enough comparable peer samples; armor median from 9 sample(s) within 10 ilvl |
| 28146 | Courier's Wraps | 10 | Uncommon | wrist | caster | no_peer_skip | 0 | 0.0 | normal stats unchanged: not enough comparable peer samples; armor median from 9 sample(s) within 10 ilvl |
| 28147 | Tranquillien Scout's Bracers | 10 | Uncommon | wrist | physical | no_peer_skip | 0 | 0.0 | normal stats unchanged: not enough comparable peer samples; armor unchanged: not enough comparable armor samples |
| 28148 | Bronze Mail Bracers | 10 | Uncommon | wrist | physical | no_peer_skip | 0 | 0.0 | normal stats unchanged: not enough comparable peer samples; armor unchanged: not enough comparable armor samples |
| 2281 | Rodentia Flint Axe | 11 | Uncommon | weapon_one_hand | healer | no_peer_skip | 0 | 0.0 | normal stats unchanged: not enough comparable peer samples; weapon DPS median from 9 sample(s) within 5 ilvl |
| 2957 | Journeyman's Vest | 11 | Uncommon | chest | healer | peer_+/-5_ilvl | 9 | -33.333 | SQL blocked: peer target requires 50.0% visible budget change; visible stat target clamped to +/-20% safety cap; rounding remained outside configured tolerance; armor unchanged: within +/-15% peer tolerance from 8 sample(s) |
| 2961 | Burnt Leather Vest | 11 | Uncommon | chest | healer | no_peer_skip | 0 | 0.0 | normal stats unchanged: not enough comparable peer samples; armor unchanged: within +/-15% peer tolerance from 17 sample(s) |

## Weapons Changed

| entry | name | item_level | quality_label | subclass_label | old_dps | new_dps | action | warnings |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 1917 | Jeweled Dagger | 10 | Uncommon | Dagger | 5.312 | 7.188 | report-only | normal stats unchanged: within +/-15% peer tolerance; weapon DPS median from 18 sample(s) within 5 ilvl; SQL refused: changed item count exceeds safety.max_sql_updates; pass --allow-large-run |
| 9513 | Ley Staff | 10 | Uncommon | Staff | 7.069 | 11.379 | report-only | SQL blocked: peer target requires 200.0% visible budget change; visible stat target clamped to +/-20% safety cap; rounding remained outside configured tolerance; weapon DPS median from 17 sample(s) within 10 ilvl |
| 9514 | Arcane Staff | 10 | Uncommon | Staff | 7.069 | 11.379 | report-only | SQL blocked: peer target requires 200.0% visible budget change; visible stat target clamped to +/-20% safety cap; rounding remained outside configured tolerance; weapon DPS median from 17 sample(s) within 10 ilvl |
| 9602 | Brushwood Blade | 10 | Uncommon | Two-hand Sword | 7.167 | 11.333 | report-only | SQL blocked: peer target requires 350.0% visible budget change; visible stat target clamped to +/-20% safety cap; rounding remained outside configured tolerance; weapon DPS median from 15 sample(s) within 10 ilvl |
| 9603 | Gritroot Staff | 10 | Uncommon | Staff | 6.923 | 11.346 | report-only | SQL blocked: peer target requires 350.0% visible budget change; visible stat target clamped to +/-20% safety cap; rounding remained outside configured tolerance; weapon DPS median from 17 sample(s) within 10 ilvl |
| 18957 | Brushwood Blade | 10 | Uncommon | Sword | 5.333 | 7.333 | report-only | normal stats unchanged: within +/-15% peer tolerance; weapon DPS median from 12 sample(s) within 5 ilvl; SQL refused: changed item count exceeds safety.max_sql_updates; pass --allow-large-run |
| 23423 | Mercenary Greatsword | 10 | Uncommon | Two-hand Sword | 7.069 | 11.379 | report-only | SQL blocked: peer target requires 100.0% visible budget change; visible stat target clamped to +/-20% safety cap; rounding remained outside configured tolerance; weapon DPS median from 15 sample(s) within 10 ilvl |
| 23429 | Mercenary Clout | 10 | Uncommon | Two-hand Mace | 7.143 | 9.286 | report-only | SQL blocked: peer target requires 100.0% visible budget change; visible stat target clamped to +/-20% safety cap; rounding remained outside configured tolerance; weapon DPS median from 10 sample(s) within 10 ilvl |
| 23430 | Mercenary Sword | 10 | Uncommon | Sword | 5.417 | 7.292 | report-only | normal stats unchanged: within +/-15% peer tolerance; weapon DPS median from 12 sample(s) within 5 ilvl; SQL refused: changed item count exceeds safety.max_sql_updates; pass --allow-large-run |
| 23431 | Mercenary Stiletto | 10 | Uncommon | Dagger | 5.333 | 7.333 | report-only | normal stats unchanged: within +/-15% peer tolerance; weapon DPS median from 18 sample(s) within 5 ilvl; SQL refused: changed item count exceeds safety.max_sql_updates; pass --allow-large-run |
| 816 | Small Hand Blade | 11 | Uncommon | Dagger | 6.0 | 7.333 | report-only | normal stats unchanged: within +/-15% peer tolerance; weapon DPS median from 18 sample(s) within 5 ilvl; SQL refused: changed item count exceeds safety.max_sql_updates; pass --allow-large-run |
| 2281 | Rodentia Flint Axe | 11 | Uncommon | Axe | 6.0 | 7.75 | report-only | normal stats unchanged: not enough comparable peer samples; weapon DPS median from 9 sample(s) within 5 ilvl |
| 3223 | Frostmane Scepter | 11 | Uncommon | Mace | 5.909 | 7.727 | report-only | normal stats unchanged: within +/-15% peer tolerance; weapon DPS median from 17 sample(s) within 5 ilvl; SQL refused: changed item count exceeds safety.max_sql_updates; pass --allow-large-run |
| 4939 | Steady Bastard Sword | 11 | Uncommon | Two-hand Sword | 7.593 | 11.481 | report-only | SQL blocked: peer target requires 58.5% visible budget change; visible stat target clamped to +/-20% safety cap; rounding remained outside configured tolerance; weapon DPS median from 15 sample(s) within 10 ilvl |
| 4947 | Jagged Dagger | 11 | Uncommon | Dagger | 6.0 | 7.333 | report-only | normal stats unchanged: within +/-15% peer tolerance; weapon DPS median from 18 sample(s) within 5 ilvl; SQL refused: changed item count exceeds safety.max_sql_updates; pass --allow-large-run |
| 4948 | Stinging Mace | 11 | Uncommon | Mace | 5.87 | 7.609 | report-only | normal stats unchanged: within +/-15% peer tolerance; weapon DPS median from 17 sample(s) within 5 ilvl; SQL refused: changed item count exceeds safety.max_sql_updates; pass --allow-large-run |
| 24339 | Stung | 11 | Uncommon | Sword | 6.0 | 7.6 | report-only | normal stats unchanged: within +/-15% peer tolerance; weapon DPS median from 15 sample(s) within 5 ilvl; SQL refused: changed item count exceeds safety.max_sql_updates; pass --allow-large-run |
| 26049 | Old Elekk Prod | 11 | Uncommon | Mace | 5.833 | 7.5 | report-only | normal stats unchanged: within +/-15% peer tolerance; weapon DPS median from 17 sample(s) within 5 ilvl; SQL refused: changed item count exceeds safety.max_sql_updates; pass --allow-large-run |
| 26053 | Elekk Handler's Blade | 11 | Uncommon | Sword | 6.0 | 7.5 | report-only | normal stats unchanged: within +/-15% peer tolerance; weapon DPS median from 15 sample(s) within 5 ilvl; SQL refused: changed item count exceeds safety.max_sql_updates; pass --allow-large-run |
| 27389 | Surplus Bastard Sword | 11 | Uncommon | Two-hand Sword | 7.812 | 11.406 | report-only | SQL blocked: peer target requires 58.5% visible budget change; visible stat target clamped to +/-20% safety cap; rounding remained outside configured tolerance; weapon DPS median from 15 sample(s) within 10 ilvl |
| 33791 | Heavy Copper Longsword | 11 | Uncommon | Sword | 6.0 | 7.6 | report-only | normal stats unchanged: within +/-15% peer tolerance; weapon DPS median from 15 sample(s) within 5 ilvl; SQL refused: changed item count exceeds safety.max_sql_updates; pass --allow-large-run |
| 4303 | Cranial Thumper | 12 | Uncommon | Mace | 6.429 | 7.5 | report-only | normal stats unchanged: within +/-15% peer tolerance; weapon DPS median from 21 sample(s) within 5 ilvl; SQL refused: changed item count exceeds safety.max_sql_updates; pass --allow-large-run |
| 4974 | Compact Fighting Knife | 12 | Uncommon | Dagger | 6.333 | 7.333 | report-only | normal stats unchanged: within +/-15% peer tolerance; weapon DPS median from 19 sample(s) within 5 ilvl; SQL refused: changed item count exceeds safety.max_sql_updates; pass --allow-large-run |
| 3488 | Copper Battle Axe | 13 | Uncommon | Two-hand Axe | 9.062 | 12.969 | report-only | SQL blocked: peer target requires 33.3% visible budget change; visible stat target clamped to +/-20% safety cap; rounding remained outside configured tolerance; final visible stat budget adjusted to stay within +/-20% safety cap; weapon DPS median from 14 sample(s) within 10 ilvl |
| 3586 | Logsplitter | 16 | Uncommon | Two-hand Axe | 10.833 | 12.639 | report-only | normal stats unchanged: within +/-15% peer tolerance; weapon DPS median from 12 sample(s) within 5 ilvl; SQL refused: changed item count exceeds safety.max_sql_updates; pass --allow-large-run |
| 27641 | Vindicator's Walking Stick | 16 | Uncommon | Staff | 10.517 | 10.517 | report-only | SQL blocked: peer target requires 26.7% visible budget change; visible stat target clamped to +/-20% safety cap; final visible stat budget adjusted to stay within +/-20% safety cap; weapon DPS unchanged: within +/-15% peer tolerance from 15 sample(s) |
| 1405 | Foamspittle Staff | 17 | Uncommon | Staff | 11.406 | 11.406 | report-only | weapon DPS unchanged: within +/-15% peer tolerance from 16 sample(s); SQL refused: changed item count exceeds safety.max_sql_updates; pass --allow-large-run |
| 3154 | Thelsamar Axe | 18 | Uncommon | Axe | 9.048 | 7.619 | report-only | normal stats unchanged: within +/-15% peer tolerance; weapon DPS median from 13 sample(s) within 5 ilvl; SQL refused: changed item count exceeds safety.max_sql_updates; pass --allow-large-run |
| 6186 | Trogg Slicer | 18 | Uncommon | Two-hand Sword | 11.912 | 11.912 | report-only | weapon DPS unchanged: within +/-15% peer tolerance from 12 sample(s); SQL refused: changed item count exceeds safety.max_sql_updates; pass --allow-large-run |
| 65492 | Broadsword of the Crown | 18 | Rare | Sword | 12.302 | 15.625 | report-only | special weapon stat budget excluded from normal visible stat budget; weapon DPS median from 9 sample(s) within 10 ilvl; SQL refused: changed item count exceeds safety.max_sql_updates; pass --allow-large-run |

## Armor Changed

| entry | name | item_level | quality_label | subclass_label | old_armor | new_armor | action | warnings |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 16604 | Moon Robes of Elune | 5 | Uncommon | Cloth | 10 | 15 | report-only | SQL blocked: peer target requires 100.0% visible budget change; visible stat target clamped to +/-20% safety cap; rounding remained outside configured tolerance; armor median from 10 sample(s) within 5 ilvl |
| 16605 | Friar's Robes of the Light | 5 | Uncommon | Cloth | 10 | 15 | report-only | SQL blocked: peer target requires 100.0% visible budget change; visible stat target clamped to +/-20% safety cap; rounding remained outside configured tolerance; armor median from 10 sample(s) within 5 ilvl |
| 16606 | Juju Hex Robes | 5 | Uncommon | Cloth | 10 | 15 | report-only | SQL blocked: peer target requires 100.0% visible budget change; visible stat target clamped to +/-20% safety cap; rounding remained outside configured tolerance; armor median from 10 sample(s) within 5 ilvl |
| 16607 | Acolyte's Sacrificial Robes | 5 | Uncommon | Cloth | 10 | 15 | report-only | SQL blocked: peer target requires 100.0% visible budget change; visible stat target clamped to +/-20% safety cap; rounding remained outside configured tolerance; armor median from 10 sample(s) within 5 ilvl |
| 23924 | Robes of Silvermoon | 5 | Uncommon | Cloth | 10 | 15 | report-only | SQL blocked: peer target requires 100.0% visible budget change; visible stat target clamped to +/-20% safety cap; rounding remained outside configured tolerance; armor median from 10 sample(s) within 5 ilvl |
| 23931 | Azure Watch Robes | 5 | Uncommon | Cloth | 10 | 15 | report-only | SQL blocked: peer target requires 100.0% visible budget change; visible stat target clamped to +/-20% safety cap; rounding remained outside configured tolerance; armor median from 10 sample(s) within 5 ilvl |
| 2958 | Journeyman's Pants | 10 | Uncommon | Cloth | 17 | 26 | report-only | SQL blocked: peer target requires 500.0% visible budget change; visible stat target clamped to +/-20% safety cap; rounding remained outside configured tolerance; armor median from 22 sample(s) within 10 ilvl |
| 3471 | Copper Chain Vest | 10 | Uncommon | Mail | 108 | 129 | report-only | SQL blocked: peer target requires 301.0% visible budget change; visible stat target clamped to +/-20% safety cap; rounding remained outside configured tolerance; armor median from 11 sample(s) within 5 ilvl |
| 24107 | Ravager Chitin Tunic | 10 | Uncommon | Mail | 108 | 129 | report-only | SQL blocked: peer target requires 301.0% visible budget change; visible stat target clamped to +/-20% safety cap; rounding remained outside configured tolerance; armor median from 11 sample(s) within 5 ilvl |
| 24109 | Thick Ravager Belt | 10 | Uncommon | Cloth | 11 | 17 | report-only | SQL blocked: peer target requires 317.0% visible budget change; visible stat target clamped to +/-20% safety cap; rounding remained outside configured tolerance; armor median from 14 sample(s) within 10 ilvl |
| 24112 | Kurkenstoks | 10 | Uncommon | Cloth | 13 | 21 | report-only | normal stats unchanged: not enough comparable peer samples; armor median from 11 sample(s) within 10 ilvl |
| 24113 | Cowlen's Bracers of Kinship | 10 | Uncommon | Cloth | 8 | 14 | report-only | normal stats unchanged: not enough comparable peer samples; armor median from 9 sample(s) within 10 ilvl |
| 28146 | Courier's Wraps | 10 | Uncommon | Cloth | 8 | 14 | report-only | normal stats unchanged: not enough comparable peer samples; armor median from 9 sample(s) within 10 ilvl |
| 2965 | Warrior's Tunic | 11 | Uncommon | Mail | 115 | 144 | report-only | SQL blocked: peer target requires 50.0% visible budget change; visible stat target clamped to +/-20% safety cap; rounding remained outside configured tolerance; final visible stat budget adjusted to stay within +/-20% safety cap; armor median from 15 sample(s) within 5 ilvl |
| 10554 | Foreman Pants | 11 | Uncommon | Cloth | 18 | 23 | report-only | normal stats unchanged: not enough comparable peer samples; armor median from 11 sample(s) within 5 ilvl |
| 24340 | Vandril's Hand Me Down Pants | 11 | Uncommon | Cloth | 18 | 23 | report-only | SQL blocked: peer target requires 500.0% visible budget change; visible stat target clamped to +/-20% safety cap; rounding remained outside configured tolerance; armor median from 11 sample(s) within 5 ilvl |
| 24341 | Fortified Oven Mitts | 11 | Uncommon | Mail | 72 | 101 | report-only | SQL blocked: peer target requires 383.5% visible budget change; visible stat target clamped to +/-20% safety cap; rounding remained outside configured tolerance; armor median from 13 sample(s) within 10 ilvl |
| 26031 | Elekk Rider's Mail | 11 | Uncommon | Mail | 115 | 144 | report-only | SQL blocked: peer target requires 79.6% visible budget change; visible stat target clamped to +/-20% safety cap; rounding remained outside configured tolerance; armor median from 15 sample(s) within 5 ilvl |
| 10549 | Rancher's Trousers | 12 | Uncommon | Cloth | 20 | 24 | report-only | SQL blocked: peer target requires 259.3% visible budget change; visible stat target clamped to +/-20% safety cap; rounding remained outside configured tolerance; armor median from 13 sample(s) within 5 ilvl |
| 26006 | Crystal-Flecked Pants | 12 | Uncommon | Cloth | 20 | 24 | report-only | normal stats unchanged: not enough comparable peer samples; armor median from 13 sample(s) within 5 ilvl |
| 27399 | Stillpine Defender | 12 | Uncommon | Shield | 239 | 328 | report-only | SQL blocked: peer target requires 198.5% visible budget change; visible stat target clamped to +/-20% safety cap; rounding remained outside configured tolerance; armor median from 11 sample(s) within 5 ilvl |
| 28144 | Troll Handler Gloves | 12 | Uncommon | Cloth | 14 | 17 | report-only | SQL blocked: peer target requires 100.0% visible budget change; visible stat target clamped to +/-20% safety cap; rounding remained outside configured tolerance; armor median from 8 sample(s) within 5 ilvl |
| 28153 | Farstrider's Shield | 12 | Uncommon | Shield | 239 | 328 | report-only | SQL blocked: peer target requires 198.5% visible budget change; visible stat target clamped to +/-20% safety cap; rounding remained outside configured tolerance; armor median from 11 sample(s) within 5 ilvl |
| 26008 | Scholar's Gloves | 13 | Uncommon | Cloth | 15 | 18 | report-only | SQL blocked: peer target requires 100.0% visible budget change; visible stat target clamped to +/-20% safety cap; rounding remained outside configured tolerance; armor median from 11 sample(s) within 5 ilvl |
| 26014 | Jessera's Fungus Lined Cuffs | 13 | Uncommon | Cloth | 11 | 14 | report-only | normal stats unchanged: not enough comparable peer samples; armor median from 10 sample(s) within 10 ilvl |
| 26028 | Jessera's Fungus Lined Bands | 13 | Uncommon | Leather | 28 | 33 | report-only | normal stats unchanged: not enough comparable peer samples; armor median from 8 sample(s) within 10 ilvl |
| 26034 | Protective Field Gloves | 13 | Uncommon | Mail | 81 | 105 | report-only | SQL blocked: peer target requires 199.4% visible budget change; visible stat target clamped to +/-20% safety cap; rounding remained outside configured tolerance; armor median from 16 sample(s) within 10 ilvl |
| 26030 | Jessera's Fungus Lined Hauberk | 16 | Uncommon | Mail | 181 | 144 | report-only | normal stats unchanged: within +/-15% peer tolerance; armor median from 18 sample(s) within 5 ilvl; SQL refused: changed item count exceeds safety.max_sql_updates; pass --allow-large-run |
| 6731 | Ironforge Breastplate | 20 | Uncommon | Mail | 198 | 162 | report-only | final visible stat budget adjusted to stay within +/-20% safety cap; armor median from 20 sample(s) within 5 ilvl; SQL refused: changed item count exceeds safety.max_sql_updates; pass --allow-large-run |
| 6468 | Deviate Scale Belt | 23 | Rare | Leather | 51 | 61 | report-only | normal stats unchanged: not enough comparable peer samples; armor median from 11 sample(s) within 10 ilvl |

## Items With Sockets

| entry | name | item_level | quality_label | slot_key | socket_budget | action | warnings |
| --- | --- | --- | --- | --- | --- | --- | --- |
| 23229 | Sword of Sockety Goodness | 41 | Uncommon | weapon_one_hand | 16.0 | report-only | socket budget counted; socket colors and socket bonus unchanged; target visible budget raised by configured minimum; SQL blocked: peer target requires 86.4% visible budget change; visible stat target clamped to +/-20% safety cap; rounding remained outside configured tolerance; final visible stat budget adjusted to stay within +/-20% safety cap |
| 27830 | Circlet of the Victor | 80 | Rare | finger | 8.0 | unchanged | socket budget counted; socket colors and socket bonus unchanged; normal stats unchanged: not enough comparable peer samples |
| 27832 | Band of the Victor | 80 | Rare | finger | 8.0 | unchanged | socket budget counted; socket colors and socket bonus unchanged; normal stats unchanged: not enough comparable peer samples |
| 27833 | Band of the Victor | 80 | Rare | finger | 8.0 | unchanged | socket budget counted; socket colors and socket bonus unchanged; normal stats unchanged: not enough comparable peer samples |
| 27834 | Circlet of the Victor | 80 | Rare | finger | 8.0 | unchanged | socket budget counted; socket colors and socket bonus unchanged; normal stats unchanged: not enough comparable peer samples |
| 24021 | Light-Touched Breastplate | 85 | Rare | chest | 24.0 | unchanged | socket budget counted; socket colors and socket bonus unchanged; normal stats unchanged: not enough comparable peer samples; armor unchanged: not enough comparable armor samples |
| 24022 | Scale Leggings of the Skirmisher | 85 | Rare | legs | 24.0 | unchanged | socket budget counted; socket colors and socket bonus unchanged; normal stats unchanged: not enough comparable peer samples; armor unchanged: not enough comparable armor samples |
| 24046 | Kilt of Rolling Thunders | 85 | Rare | legs | 24.0 | unchanged | socket budget counted; socket colors and socket bonus unchanged; normal stats unchanged: not enough comparable peer samples; armor unchanged: not enough comparable armor samples |
| 24063 | Shifting Sash of Midnight | 85 | Rare | waist | 16.0 | unchanged | socket budget counted; socket colors and socket bonus unchanged; normal stats unchanged: not enough comparable peer samples; armor unchanged: not enough comparable armor samples |
| 24064 | Ironsole Clompers | 85 | Rare | feet | 16.0 | unchanged | socket budget counted; socket colors and socket bonus unchanged; normal stats unchanged: not enough comparable peer samples; armor unchanged: not enough comparable armor samples |
| 24083 | Lifegiver Britches | 85 | Rare | legs | 24.0 | unchanged | socket budget counted; socket colors and socket bonus unchanged; normal stats unchanged: not enough comparable peer samples; armor unchanged: not enough comparable armor samples |
| 24090 | Bloodstained Ravager Gauntlets | 85 | Rare | hands | 16.0 | unchanged | socket budget counted; socket colors and socket bonus unchanged; normal stats unchanged: not enough comparable peer samples; armor unchanged: not enough comparable armor samples |
| 24091 | Tenacious Defender | 85 | Rare | waist | 16.0 | unchanged | socket budget counted; socket colors and socket bonus unchanged; normal stats unchanged: not enough comparable peer samples; armor unchanged: not enough comparable armor samples |
| 153959 | Lightdrinker Dagger | 85 | Rare | weapon_one_hand | 8.0 | unchanged | special weapon stat budget excluded from normal visible stat budget; socket budget counted; socket colors and socket bonus unchanged; normal stats unchanged: not enough comparable peer samples; weapon damage unchanged: not enough comparable physical weapon samples |
| 24387 | Ironblade Gauntlets | 88 | Rare | hands | 16.0 | unchanged | socket budget counted; socket colors and socket bonus unchanged; normal stats unchanged: not enough comparable peer samples; armor unchanged: not enough comparable armor samples |
| 24388 | Girdle of the Gale Storm | 88 | Rare | waist | 16.0 | unchanged | socket budget counted; socket colors and socket bonus unchanged; normal stats unchanged: not enough comparable peer samples; armor unchanged: not enough comparable armor samples |
| 24391 | Kilt of the Night Strider | 88 | Rare | legs | 24.0 | unchanged | socket budget counted; socket colors and socket bonus unchanged; normal stats unchanged: not enough comparable peer samples; armor unchanged: not enough comparable armor samples |
| 24393 | Bloody Surgeon's Mitts | 88 | Rare | hands | 16.0 | unchanged | socket budget counted; socket colors and socket bonus unchanged; normal stats unchanged: not enough comparable peer samples; armor unchanged: not enough comparable armor samples |
| 24395 | Mindfire Waistband | 88 | Rare | waist | 16.0 | unchanged | socket budget counted; socket colors and socket bonus unchanged; normal stats unchanged: not enough comparable peer samples; armor unchanged: not enough comparable armor samples |
| 24396 | Vest of Vengeance | 88 | Rare | chest | 24.0 | unchanged | socket budget counted; socket colors and socket bonus unchanged; normal stats unchanged: not enough comparable peer samples; armor unchanged: not enough comparable armor samples |
| 24397 | Raiments of Divine Authority | 88 | Rare | chest | 24.0 | unchanged | socket budget counted; socket colors and socket bonus unchanged; normal stats unchanged: not enough comparable peer samples; armor unchanged: not enough comparable armor samples |
| 23362 | Hammer of the Sun | 90 | Epic | weapon_main_hand | 28.0 | unchanged | socket budget counted; socket colors and socket bonus unchanged; normal stats unchanged: not enough comparable peer samples |
| 23363 | Titanic Breastplate | 90 | Epic | chest | 36.0 | unchanged | socket budget counted; socket colors and socket bonus unchanged; normal stats unchanged: not enough comparable peer samples |
| 27715 | Circle's Stalwart Helmet | 90 | Uncommon | head | 24.0 | unchanged | socket budget counted; socket colors and socket bonus unchanged; normal stats unchanged: not enough comparable peer samples; armor unchanged: not enough comparable armor samples |
| 27717 | Expedition Forager Leggings | 90 | Uncommon | legs | 24.0 | unchanged | socket budget counted; socket colors and socket bonus unchanged; normal stats unchanged: not enough comparable peer samples; armor unchanged: not enough comparable armor samples |
| 31657 | Chemise of Rebirth | 90 | Uncommon | chest | 24.0 | unchanged | socket budget counted; socket colors and socket bonus unchanged; normal stats unchanged: not enough comparable peer samples; armor unchanged: not enough comparable armor samples |
| 31658 | Scout's Hood | 90 | Uncommon | head | 24.0 | unchanged | socket budget counted; socket colors and socket bonus unchanged; SQL blocked: exact peer sample count is below configured minimum; armor unchanged: within +/-15% peer tolerance from 9 sample(s) |
| 24357 | Vest of Living Lightning | 91 | Rare | chest | 24.0 | unchanged | socket budget counted; socket colors and socket bonus unchanged; normal stats unchanged: not enough comparable peer samples; armor unchanged: not enough comparable armor samples |
| 24363 | Unscarred Breastplate | 91 | Rare | chest | 24.0 | unchanged | socket budget counted; socket colors and socket bonus unchanged; normal stats unchanged: not enough comparable peer samples; armor unchanged: within +/-15% peer tolerance from 11 sample(s) |
| 24365 | Deft Handguards | 91 | Rare | hands | 16.0 | unchanged | socket budget counted; socket colors and socket bonus unchanged; normal stats unchanged: not enough comparable peer samples; armor unchanged: not enough comparable armor samples |

## Spell Payloads Intentionally Ignored

| entry | name | item_level | quality_label | slot_key | action | warnings |
| --- | --- | --- | --- | --- | --- | --- |
| 17 | Martin Fury | 5 | 6 |  | skipped | spell payload ignored and left unchanged; normal stats unchanged: item could not be peer-classified |
| 7507 | Arcane Orb | 10 | Uncommon | held_in_off_hand | unchanged | spell payload ignored and left unchanged; normal stats unchanged: not enough comparable peer samples |
| 7508 | Ley Orb | 10 | Uncommon | held_in_off_hand | unchanged | spell payload ignored and left unchanged; normal stats unchanged: not enough comparable peer samples |
| 15866 | Veildust Medicine Bag | 15 | Uncommon | held_in_off_hand | unchanged | spell payload ignored and left unchanged; SQL blocked: peer target requires 67.0% visible budget change; visible stat target clamped to +/-20% safety cap; rounding remained outside configured tolerance |
| 28164 | Tranquillien Flamberge | 15 | Uncommon | weapon_two_hand | unchanged | spell payload ignored and left unchanged; normal stats unchanged: within +/-15% peer tolerance; weapon DPS unchanged: within +/-15% peer tolerance from 15 sample(s) |
| 825438 | Distinct Emerald Pendant | 18 | Rare | neck | unchanged | spell payload ignored and left unchanged; normal stats unchanged: not enough comparable peer samples |
| 5183 | Pulsating Hydra Heart | 20 | Rare | held_in_off_hand | unchanged | spell payload ignored and left unchanged; normal stats unchanged: not enough comparable peer samples |
| 5387 | Enchanted Moonstalker Cloak | 20 | Uncommon | cloak | unchanged | spell payload ignored and left unchanged; normal stats unchanged: within +/-15% peer tolerance; armor unchanged: within +/-15% peer tolerance from 48 sample(s) |
| 22783 | Sunwell Blade | 20 | Uncommon | weapon_one_hand | unchanged | spell payload ignored and left unchanged; target visible budget raised by configured minimum; SQL blocked: peer target requires 50.2% visible budget change; visible stat target clamped to +/-20% safety cap; rounding remained outside configured tolerance; final visible stat budget adjusted to stay within +/-20% safety cap; weapon DPS unchanged: within +/-15% peer tolerance from 24 sample(s) |
| 22784 | Sunwell Orb | 20 | Uncommon | held_in_off_hand | unchanged | spell payload ignored and left unchanged; normal stats unchanged: not enough comparable peer samples |
| 1484 | Witching Stave | 22 | Rare | weapon_two_hand | unchanged | spell payload ignored and left unchanged; normal stats unchanged: not enough comparable peer samples; weapon DPS unchanged: within +/-15% peer tolerance from 10 sample(s) |
| 5613 | Staff of the Purifier | 23 | Uncommon | weapon_two_hand | report-only | spell payload ignored and left unchanged; SQL blocked: peer target requires 25.0% visible budget change; visible stat target clamped to +/-20% safety cap; rounding remained outside configured tolerance; final visible stat budget adjusted to stay within +/-20% safety cap; weapon DPS unchanged: within +/-15% peer tolerance from 17 sample(s) |
| 30804 | Bronze Band of Force | 23 | Rare | finger | unchanged | spell payload ignored and left unchanged; normal stats unchanged: not enough comparable peer samples |
| 4444 | Black Husk Shield | 24 | Uncommon | shield | report-only | spell payload ignored and left unchanged; SQL blocked: peer target requires 22.1% visible budget change; visible stat target clamped to +/-20% safety cap; rounding remained outside configured tolerance; final visible stat budget adjusted to stay within +/-20% safety cap; armor unchanged: within +/-15% peer tolerance from 25 sample(s) |
| 2950 | Icicle Rod | 25 | Uncommon | weapon_two_hand | unchanged | spell payload ignored and left unchanged; SQL blocked: peer target requires 50.0% visible budget change; visible stat target clamped to +/-20% safety cap; rounding remained outside configured tolerance; final visible stat budget adjusted to stay within +/-20% safety cap; weapon DPS unchanged: within +/-15% peer tolerance from 12 sample(s) |
| 4317 | Phoenix Pants | 25 | Uncommon | legs | unchanged | spell payload ignored and left unchanged; SQL blocked: peer target requires 37.7% visible budget change; visible stat target clamped to +/-20% safety cap; rounding remained outside configured tolerance; final visible stat budget adjusted to stay within +/-20% safety cap; armor unchanged: within +/-15% peer tolerance from 13 sample(s) |
| 5323 | Everglow Lantern | 25 | Uncommon | held_in_off_hand | unchanged | spell payload ignored and left unchanged; SQL blocked: peer target requires 44.7% visible budget change; visible stat target clamped to +/-20% safety cap; rounding remained outside configured tolerance; final visible stat budget adjusted to stay within +/-20% safety cap |
| 21566 | Rune of Perfection | 25 | Rare |  | skipped | spell payload ignored and left unchanged; normal stats unchanged: item could not be peer-classified |
| 21568 | Rune of Duty | 25 | Rare |  | skipped | spell payload ignored and left unchanged; normal stats unchanged: item could not be peer-classified |
| 43655 | Book of Survival | 25 | Rare | held_in_off_hand | unchanged | spell payload ignored and left unchanged; normal stats unchanged: not enough comparable peer samples |
| 883922 | Stonemason's Mark | 25 | Rare |  | skipped | spell payload ignored and left unchanged; normal stats unchanged: item could not be peer-classified |
| 4446 | Blackvenom Blade | 26 | Rare | weapon_one_hand | unchanged | spell payload ignored and left unchanged; SQL blocked: peer target requires 501.0% visible budget change; visible stat target clamped to +/-20% safety cap; rounding remained outside configured tolerance; weapon DPS unchanged: within +/-15% peer tolerance from 11 sample(s) |
| 7027 | Boots of Darkness | 28 | Uncommon | feet | unchanged | spell payload ignored and left unchanged; normal stats unchanged: not enough comparable peer samples; armor unchanged: within +/-15% peer tolerance from 14 sample(s) |
| 7046 | Azure Silk Pants | 28 | Uncommon | legs | unchanged | spell payload ignored and left unchanged; normal stats unchanged: not enough comparable peer samples; armor unchanged: within +/-15% peer tolerance from 12 sample(s) |
| 13031 | Orb of Mistmantle | 28 | Rare | held_in_off_hand | unchanged | spell payload ignored and left unchanged; normal stats unchanged: not enough comparable peer samples |
| 3566 | Raptorbane Armor | 29 | Uncommon | chest | unchanged | spell payload ignored and left unchanged; normal stats unchanged: not enough comparable peer samples; armor unchanged: within +/-15% peer tolerance from 16 sample(s) |
| 4319 | Azure Silk Gloves | 29 | Uncommon | hands | unchanged | spell payload ignored and left unchanged; SQL blocked: peer target requires 200.0% visible budget change; visible stat target clamped to +/-20% safety cap; rounding remained outside configured tolerance; final visible stat budget adjusted to stay within +/-20% safety cap; armor unchanged: within +/-15% peer tolerance from 15 sample(s) |
| 7047 | Hands of Darkness | 29 | Uncommon | hands | report-only | spell payload ignored and left unchanged; SQL blocked: peer target requires 80.0% visible budget change; visible stat target clamped to +/-20% safety cap; armor unchanged: within +/-15% peer tolerance from 15 sample(s) |
| 4324 | Azure Silk Vest | 30 | Uncommon | chest | report-only | spell payload ignored and left unchanged; SQL blocked: peer target requires 31.6% visible budget change; visible stat target clamped to +/-20% safety cap; final visible stat budget adjusted to stay within +/-20% safety cap; armor unchanged: within +/-15% peer tolerance from 9 sample(s) |
| 6972 | Fire Hardened Hauberk | 30 | Rare | chest | unchanged | spell payload ignored and left unchanged; normal stats unchanged: not enough comparable peer samples; armor unchanged: within +/-15% peer tolerance from 10 sample(s) |

## Itemsets Intentionally Ignored

| entry | name | item_level | quality_label | slot_key | action | warnings |
| --- | --- | --- | --- | --- | --- | --- |
| 10400 | Blackened Defias Leggings | 18 | Uncommon | legs | unchanged | itemset ignored and left unchanged; normal stats unchanged: within +/-15% peer tolerance; armor unchanged: within +/-15% peer tolerance from 25 sample(s) |
| 10401 | Blackened Defias Gloves | 18 | Uncommon | hands | report-only | itemset ignored and left unchanged; SQL blocked: peer target requires 36.2% visible budget change; visible stat target clamped to +/-20% safety cap; rounding remained outside configured tolerance; armor unchanged: within +/-15% peer tolerance from 21 sample(s) |
| 10402 | Blackened Defias Boots | 18 | Uncommon | feet | unchanged | itemset ignored and left unchanged; normal stats unchanged: within +/-15% peer tolerance; armor unchanged: within +/-15% peer tolerance from 16 sample(s) |
| 10413 | Gloves of the Fang | 19 | Uncommon | hands | unchanged | itemset ignored and left unchanged; normal stats unchanged: within +/-15% peer tolerance; armor unchanged: within +/-15% peer tolerance from 22 sample(s) |
| 10412 | Belt of the Fang | 21 | Uncommon | waist | report-only | itemset ignored and left unchanged; SQL blocked: peer target requires 49.9% visible budget change; visible stat target clamped to +/-20% safety cap; armor unchanged: within +/-15% peer tolerance from 17 sample(s) |
| 10403 | Blackened Defias Belt | 22 | Uncommon | waist | report-only | itemset ignored and left unchanged; SQL blocked: peer target requires 20.2% visible budget change; visible stat target clamped to +/-20% safety cap; armor unchanged: within +/-15% peer tolerance from 19 sample(s) |
| 6473 | Armor of the Fang | 23 | Uncommon | chest | unchanged | itemset ignored and left unchanged; normal stats unchanged: within +/-15% peer tolerance; armor unchanged: within +/-15% peer tolerance from 20 sample(s) |
| 10410 | Leggings of the Fang | 23 | Rare | legs | unchanged | itemset ignored and left unchanged; normal stats unchanged: not enough comparable peer samples; armor unchanged: not enough comparable armor samples |
| 10411 | Footpads of the Fang | 23 | Uncommon | feet | unchanged | itemset ignored and left unchanged; normal stats unchanged: within +/-15% peer tolerance; armor unchanged: within +/-15% peer tolerance from 18 sample(s) |
| 10399 | Blackened Defias Armor | 24 | Rare | chest | unchanged | itemset ignored and left unchanged; normal stats unchanged: not enough comparable peer samples; armor unchanged: not enough comparable armor samples |
| 10332 | Scarlet Boots | 35 | Rare | feet | report-only | itemset ignored and left unchanged; SQL blocked: peer target requires 43.3% visible budget change; visible stat target clamped to +/-20% safety cap; armor unchanged: within +/-15% peer tolerance from 11 sample(s) |
| 10333 | Scarlet Wristguards | 36 | Uncommon | wrist | unchanged | itemset ignored and left unchanged; normal stats unchanged: not enough comparable peer samples; armor unchanged: within +/-15% peer tolerance from 9 sample(s) |
| 10329 | Scarlet Belt | 37 | Uncommon | waist | report-only | itemset ignored and left unchanged; armor unchanged: within +/-15% peer tolerance from 9 sample(s); SQL refused: changed item count exceeds safety.max_sql_updates; pass --allow-large-run |
| 10331 | Scarlet Gauntlets | 38 | Uncommon | hands | report-only | itemset ignored and left unchanged; SQL blocked: peer target requires 29.9% visible budget change; visible stat target clamped to +/-20% safety cap; armor unchanged: within +/-15% peer tolerance from 19 sample(s) |
| 10328 | Scarlet Chestpiece | 39 | Rare | chest | unchanged | itemset ignored and left unchanged; normal stats unchanged: not enough comparable peer samples; armor unchanged: within +/-15% peer tolerance from 13 sample(s) |
| 7953 | Mask of Thero-Shan | 42 | Uncommon | head | report-only | spell payload ignored and left unchanged; itemset ignored and left unchanged; SQL blocked: peer target requires 38.4% visible budget change; visible stat target clamped to +/-20% safety cap; final visible stat budget adjusted to stay within +/-20% safety cap |
| 10330 | Scarlet Leggings | 43 | Rare | legs | unchanged | itemset ignored and left unchanged; normal stats unchanged: not enough comparable peer samples; armor unchanged: within +/-15% peer tolerance from 10 sample(s) |
| 15045 | Green Dragonscale Breastplate | 52 | Rare | chest | report-only | itemset ignored and left unchanged; SQL blocked: peer target requires 24.0% visible budget change; visible stat target clamped to +/-20% safety cap; armor unchanged: within +/-15% peer tolerance from 18 sample(s) |
| 12424 | Imperial Plate Belt | 53 | Uncommon | waist | unchanged | itemset ignored and left unchanged; normal stats unchanged: within +/-15% peer tolerance; armor unchanged: within +/-15% peer tolerance from 12 sample(s) |
| 12428 | Imperial Plate Shoulders | 53 | Uncommon | shoulder | unchanged | itemset ignored and left unchanged; normal stats unchanged: not enough comparable peer samples; armor unchanged: within +/-15% peer tolerance from 16 sample(s) |
| 12425 | Imperial Plate Bracers | 54 | Uncommon | wrist | unchanged | itemset ignored and left unchanged; normal stats unchanged: not enough comparable peer samples; armor unchanged: within +/-15% peer tolerance from 13 sample(s) |
| 15046 | Green Dragonscale Leggings | 54 | Rare | legs | report-only | itemset ignored and left unchanged; SQL blocked: peer target requires 86.8% visible budget change; visible stat target clamped to +/-20% safety cap; armor unchanged: within +/-15% peer tolerance from 8 sample(s) |
| 15067 | Ironfeather Shoulders | 54 | Rare | shoulder | report-only | itemset ignored and left unchanged; SQL blocked: peer target requires 27.7% visible budget change; visible stat target clamped to +/-20% safety cap; final visible stat budget adjusted to stay within +/-20% safety cap; armor unchanged: within +/-15% peer tolerance from 22 sample(s) |
| 15057 | Stormshroud Pants | 55 | Rare | legs | unchanged | itemset ignored and left unchanged; normal stats unchanged: not enough comparable peer samples; armor unchanged: within +/-15% peer tolerance from 14 sample(s) |
| 21998 | Gauntlets of Heroism | 55 | Epic | hands | unchanged | spell payload ignored and left unchanged; itemset ignored and left unchanged; normal stats unchanged: not enough comparable peer samples; armor unchanged: not enough comparable armor samples |
| 22006 | Darkmantle Gloves | 55 | Epic | hands | unchanged | itemset ignored and left unchanged; normal stats unchanged: not enough comparable peer samples; armor unchanged: not enough comparable armor samples |
| 22015 | Beastmaster's Gloves | 55 | Epic | hands | unchanged | spell payload ignored and left unchanged; itemset ignored and left unchanged; normal stats unchanged: not enough comparable peer samples; armor unchanged: not enough comparable armor samples |
| 22066 | Sorcerer's Gloves | 55 | Epic | hands | unchanged | spell payload ignored and left unchanged; itemset ignored and left unchanged; normal stats unchanged: not enough comparable peer samples; armor unchanged: not enough comparable armor samples |
| 22077 | Deathmist Wraps | 55 | Epic | hands | unchanged | spell payload ignored and left unchanged; itemset ignored and left unchanged; normal stats unchanged: not enough comparable peer samples; armor unchanged: not enough comparable armor samples |
| 22081 | Virtuous Gloves | 55 | Epic | hands | unchanged | itemset ignored and left unchanged; normal stats unchanged: not enough comparable peer samples; armor unchanged: not enough comparable armor samples |

## Unknown Stat Types

None.

## Calibration Snapshot

Grouped by expansion, item-level band, exact item level, quality, slot, inferred role, and peer group. Full data is in the calibration CSV.

| count | expansion_band | ilvl_band | item_level | quality_label | slot_key | role | peer_group | median_visible_budget | median_special_weapon_budget | median_total_budget_with_sockets | median_target_budget | median_percent_over_under | peer_outlier_count | sql_blocked_count |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 26 | classic_leveling | 010-019 | 15 | Uncommon | weapon_one_hand | generic | quality=2;slot=weapon_one_hand;armor=none;weapon=weapon_one_hand:physical;role=generic | 0.67 | 0.0 | 0.67 | 0.67 | 0.0 | 0 | 0 |
| 26 | wrath_t8 | 220-239 | 232 | Epic | weapon_one_hand | physical | quality=4;slot=weapon_one_hand;armor=none;weapon=weapon_one_hand:physical;role=physical | 145.485 | 37.0 | 149.16 | 149.16 | 0.0 | 0 | 0 |
| 25 | wrath_t7 | 200-219 | 213 | Epic | weapon_two_hand | generic | quality=4;slot=weapon_two_hand;armor=none;weapon=weapon_two_hand:physical;role=generic | 331.36 | 0.0 | 331.36 | 331.36 | 0.0 | 4 | 4 |
| 21 | burning_crusade | 105-119 | 115 | Rare | weapon_one_hand | generic | quality=3;slot=weapon_one_hand;armor=none;weapon=weapon_one_hand:physical;role=generic | 42.06 | 13.0 | 42.06 | 42.06 | 0.0 | 4 | 4 |
| 20 | burning_crusade | 105-119 | 115 | Rare | chest | caster | quality=3;slot=chest;armor=1;weapon=none;role=caster | 85.77 | 0.0 | 109.77 | 109.77 | 0.0 | 2 | 1 |
| 18 | burning_crusade | 105-119 | 115 | Rare | head | caster | quality=3;slot=head;armor=1;weapon=none;role=caster | 85.77 | 0.0 | 105.77 | 105.77 | 0.0 | 2 | 2 |
| 18 | burning_crusade | 105-119 | 115 | Rare | weapon_two_hand | physical | quality=3;slot=weapon_two_hand;armor=none;weapon=weapon_two_hand:physical;role=physical | 65.635 | 39.0 | 70.925 | 70.925 | 1.075 | 17 | 14 |
| 18 | wrath_t9 | 240-259 | 245 | Epic | weapon_one_hand | physical | quality=4;slot=weapon_one_hand;armor=none;weapon=weapon_one_hand:physical;role=physical | 164.86 | 39.0 | 174.86 | 174.86 | 0.0 | 0 | 0 |
| 17 | burning_crusade | 105-119 | 115 | Rare | legs | caster | quality=3;slot=legs;armor=1;weapon=none;role=caster | 110.1 | 0.0 | 110.1 | 110.1 | 0.0 | 0 | 0 |
| 17 | burning_crusade | 105-119 | 115 | Rare | shoulder | caster | quality=3;slot=shoulder;armor=1;weapon=none;role=caster | 70.0 | 0.0 | 86.0 | 86.0 | 0.0 | 1 | 1 |
| 17 | wrath_t8 | 220-239 | 232 | Epic | hands | generic | quality=4;slot=hands;armor=4;weapon=none;role=generic | 274.34 | 0.0 | 290.34 | 290.34 | 0.0 | 0 | 0 |
| 17 | wrath_t9 | 240-259 | 245 | Epic | weapon_two_hand | physical | quality=4;slot=weapon_two_hand;armor=none;weapon=weapon_two_hand:physical;role=physical | 391.12 | 71.0 | 413.85 | 410.485 | 0.82 | 1 | 1 |
| 16 | burning_crusade | 105-119 | 115 | Rare | weapon_one_hand | physical | quality=3;slot=weapon_one_hand;armor=none;weapon=weapon_one_hand:physical;role=physical | 26.39 | 14.5 | 26.39 | 26.39 | 0.814 | 11 | 7 |
| 16 | wrath_t8 | 220-239 | 232 | Epic | head | tank | quality=4;slot=head;armor=4;weapon=none;role=tank | 380.055 | 0.0 | 420.055 | 420.055 | 0.001 | 0 | 0 |
| 16 | wrath_t8 | 220-239 | 232 | Epic | weapon_two_hand | physical | quality=4;slot=weapon_two_hand;armor=none;weapon=weapon_two_hand:physical;role=physical | 348.13 | 74.0 | 357.375 | 357.375 | 0.0 | 1 | 0 |
| 15 | burning_crusade | 105-119 | 115 | Rare | weapon_main_hand | caster | quality=3;slot=weapon_main_hand;armor=none;weapon=weapon_main_hand:caster;role=caster | 38.05 | 104.06 | 39.1 | 38.575 | 1.361 | 4 | 3 |
| 15 | classic_endgame | 060-089 | 75 | Epic | weapon_two_hand | physical | quality=4;slot=weapon_two_hand;armor=none;weapon=weapon_two_hand:physical;role=physical | 33.39 | 0.0 | 33.39 | 33.39 | 0.0 | 0 | 0 |
| 15 | wrath_pre_raid | 165-199 | 174 | Uncommon | weapon_one_hand | physical | quality=2;slot=weapon_one_hand;armor=none;weapon=weapon_one_hand:physical;role=physical | 45.11 | 11.5 | 45.11 | 49.88 | -9.563 | 13 | 13 |
| 15 | wrath_t8 | 220-239 | 232 | Epic | chest | tank | quality=4;slot=chest;armor=4;weapon=none;role=tank | 370.19 | 0.0 | 402.19 | 415.21 | -3.136 | 0 | 0 |
| 15 | wrath_t8 | 220-239 | 232 | Epic | shoulder | generic | quality=4;slot=shoulder;armor=4;weapon=none;role=generic | 282.11 | 0.0 | 298.11 | 298.11 | 0.0 | 0 | 0 |
| 15 | wrath_t8 | 220-239 | 232 | Epic | shoulder | tank | quality=4;slot=shoulder;armor=4;weapon=none;role=tank | 306.74 | 0.0 | 322.74 | 320.76 | 0.617 | 0 | 0 |
| 14 | burning_crusade | 105-119 | 115 | Rare | hands | caster | quality=3;slot=hands;armor=1;weapon=none;role=caster | 72.61 | 0.0 | 72.97 | 72.97 | 0.005 | 2 | 1 |
| 14 | wrath_t8 | 220-239 | 226 | Epic | finger | physical | quality=4;slot=finger;armor=0;weapon=none;role=physical | 210.085 | 0.0 | 217.89 | 217.89 | 0.221 | 1 | 1 |
| 14 | wrath_t8 | 220-239 | 232 | Epic | head | generic | quality=4;slot=head;armor=4;weapon=none;role=generic | 350.755 | 0.0 | 390.755 | 390.755 | 0.004 | 0 | 0 |
| 13 | burning_crusade | 105-119 | 115 | Rare | chest | generic | quality=3;slot=chest;armor=4;weapon=none;role=generic | 94.14 | 0.0 | 118.14 | 118.14 | 0.0 | 1 | 1 |
| 13 | burning_crusade | 105-119 | 115 | Rare | weapon_two_hand | generic | quality=3;slot=weapon_two_hand;armor=none;weapon=weapon_two_hand:physical;role=generic | 93.14 | 0.0 | 105.76 | 101.11 | 4.599 | 5 | 1 |
| 13 | burning_crusade | 135-149 | 138 | Uncommon | weapon_one_hand | physical | quality=2;slot=weapon_one_hand;armor=none;weapon=weapon_one_hand:physical;role=physical | 37.0 | 0.0 | 37.0 | 37.73 | -1.935 | 5 | 5 |
| 13 | classic_endgame | 060-089 | 65 | Rare | weapon_two_hand | physical | quality=3;slot=weapon_two_hand;armor=none;weapon=weapon_two_hand:physical;role=physical | 41.06 | 0.0 | 41.06 | 41.06 | 0.0 | 0 | 0 |
| 13 | wrath_t7 | 200-219 | 200 | Epic | shield | tank | quality=4;slot=shield;armor=shield;weapon=shield;role=tank | 175.37 | 0.0 | 175.37 | 170.055 | 3.125 | 3 | 2 |
| 13 | wrath_t8 | 220-239 | 232 | Epic | head | physical | quality=4;slot=head;armor=2;weapon=none;role=physical | 385.46 | 0.0 | 425.46 | 422.27 | 0.755 | 0 | 0 |
| 13 | wrath_t8 | 220-239 | 232 | Epic | shoulder | healer | quality=4;slot=shoulder;armor=1;weapon=none;role=healer | 306.9 | 0.0 | 322.9 | 309.94 | 4.181 | 0 | 0 |
| 13 | wrath_t9 | 240-259 | 251 | Epic | hands | generic | quality=4;slot=hands;armor=4;weapon=none;role=generic | 329.74 | 0.0 | 345.74 | 350.97 | -1.49 | 0 | 0 |
| 12 | burning_crusade | 135-149 | 146 | Uncommon | weapon_one_hand | physical | quality=2;slot=weapon_one_hand;armor=none;weapon=weapon_one_hand:physical;role=physical | 42.0 | 0.0 | 42.0 | 42.0 | 0.0 | 5 | 5 |
| 12 | burning_crusade | 135-149 | 146 | Epic | weapon_off_hand | generic | quality=4;slot=weapon_off_hand;armor=none;weapon=weapon_off_hand:physical;role=generic | 68.1 | 17.0 | 68.1 | 68.1 | 0.0 | 4 | 4 |
| 12 | wrath_t7 | 200-219 | 200 | Epic | head | generic | quality=4;slot=head;armor=4;weapon=none;role=generic | 245.81 | 0.0 | 285.81 | 285.81 | 0.006 | 0 | 0 |
| 12 | wrath_t7 | 200-219 | 219 | Epic | weapon_one_hand | physical | quality=4;slot=weapon_one_hand;armor=none;weapon=weapon_one_hand:physical;role=physical | 130.12 | 35.0 | 130.12 | 130.12 | 0.005 | 0 | 0 |
| 12 | wrath_t8 | 220-239 | 232 | Epic | hands | tank | quality=4;slot=hands;armor=4;weapon=none;role=tank | 292.24 | 0.0 | 308.24 | 308.24 | 0.279 | 0 | 0 |
| 12 | wrath_t8 | 220-239 | 232 | Epic | legs | physical | quality=4;slot=legs;armor=4;weapon=none;role=physical | 356.79 | 0.0 | 388.79 | 388.79 | 0.0 | 0 | 0 |
| 12 | wrath_t8 | 220-239 | 232 | Epic | legs | tank | quality=4;slot=legs;armor=4;weapon=none;role=tank | 402.19 | 0.0 | 434.19 | 434.19 | 0.0 | 4 | 0 |
| 12 | wrath_t9 | 240-259 | 245 | Epic | chest | tank | quality=4;slot=chest;armor=4;weapon=none;role=tank | 452.6 | 0.0 | 484.6 | 484.6 | 0.001 | 0 | 0 |
