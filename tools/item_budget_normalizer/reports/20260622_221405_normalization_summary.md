# Item Budget Normalizer Summary

- Generated: 2026-06-22T22:14:06
- Input CSV: `F:\Downloads\First_pass_normalization.csv`
- Config: `C:\AzerothCore\Burning_ Legacy_Server\AzerothCore-wotlk-with-NPCBots\tools\item_budget_normalizer\item_budget_config.yml`
- SQL generation: enabled
- Origin manifest: `F:\Downloads\private_server_item_origin_builder\private_server_item_origin_builder\item_origin_manifests\item_origin_manifest.csv`
- SQL origin filter: custom_only `False`, labels `['vanilla']`, expansions `none`
- Target mode: `peer`
- Peer minimum samples: `8`; outlier tolerance: `+/-15.0%`
- Calibration sanity minimum samples: `10`
- Visible budget per-pass cap: `+/-20.0%`; allow large changes: `False`
- Allow special-effect/itemset SQL: `False`
- Calibration sanity: sane; worst grouped median `2.156%`
- Filters: min ilvl `60`, max ilvl `None`, required level `None`, qualities `any`

## Output Files

- Before/after CSV: `tools\item_budget_normalizer\reports\20260622_221405_normalization_report.csv`
- Calibration CSV: `tools\item_budget_normalizer\reports\20260622_221405_calibration.csv`
- Update SQL: `tools\item_budget_normalizer\reports\20260622_221405_normalization_updates.sql`
- Rollback SQL: `tools\item_budget_normalizer\reports\20260622_221405_normalization_rollback.sql`

## Counts

- Report rows: 14918
- Skipped: 553
- Report-only changes: 1027
- Updated SQL rows: 109
- Unchanged: 13229
- Would change if SQL were enabled: 1136
- Special weapon-stat rows: 1612
- Peer outliers: 1468
- SQL-blocked rows: 14012
- Low-sample calibration groups ignored by sanity: 7671

## Origin Coverage

| origin_label | count |
| --- | --- |
| custom_or_unknown | 1404 |
| tbc | 4406 |
| vanilla | 2225 |
| wrath | 6883 |

## Skipped Items And Why

| Reason | Count |
| --- | --- |
| cannot classify inventory slot | 473 |
| trinket excluded | 431 |
| not armor or weapon class | 53 |
| name matched exclusion pattern Test  | 32 |
| quality 1 not configured | 19 |
| name matched exclusion pattern [PH] | 7 |
| duplicate stat type 32 in slot 4 | 6 |
| non-contiguous stat slot 4 | 5 |
| duplicate stat type 38 in slot 7 | 4 |
| duplicate stat type 32 in slot 7 | 3 |
| quality 6 not configured | 3 |
| stat slot 1 has negative value -30 | 3 |
| duplicate stat type 45 in slot 5 | 2 |
| duplicate stat type 45 in slot 6 | 2 |
| non-contiguous stat slot 3 | 2 |
| duplicate stat type 31 in slot 6 | 1 |
| duplicate stat type 32 in slot 2 | 1 |
| duplicate stat type 38 in slot 3 | 1 |
| name matched exclusion pattern Monster - | 1 |
| non-contiguous stat slot 5 | 1 |
| non-contiguous stat slot 6 | 1 |
| stat slot 2 has negative value -10 | 1 |
| stat slot 3 has negative value -10 | 1 |
| stat slot 3 has negative value -35 | 1 |
| stat slot 3 has negative value -42 | 1 |
| stat slot 3 has negative value -53 | 1 |

## Most Overbudget Before Normalization

| entry | name | origin_label | pvp_family | item_level | quality_label | slot_key | role | target_source | target_sample_count | percent_over_under_before | action |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 28426 | Blazeguard | tbc | none | 123 | Epic | weapon_one_hand | generic | peer_exact_ilvl_low_sample | 1 | 741.625 | unchanged |
| 27892 | Cloak of the Inciter | tbc | none | 112 | Rare | cloak | generic | peer_exact_ilvl_low_sample | 1 | 634.453 | unchanged |
| 30313 | Staff of Disintegration | tbc | none | 175 | Legendary | weapon_two_hand | physical | peer_exact_ilvl_low_sample | 1 | 353.616 | unchanged |
| 28427 | Blazefury | tbc | none | 136 | Epic | weapon_one_hand | generic | peer_exact_ilvl_low_sample | 1 | 338.02 | unchanged |
| 51876 | Abomination Knuckles | wrath | none | 264 | Epic | weapon_main_hand | physical | peer_exact_ilvl_low_sample | 1 | 322.907 | unchanged |
| 21459 | Crossbow of Imminent Doom | vanilla | none | 72 | Epic | ranged | physical | peer_+/-5_ilvl | 9 | 320.398 | report-only |
| 24106 | Thick Felsteel Necklace | tbc | none | 113 | Rare | neck | generic | peer_+/-6_ilvl | 9 | 290.713 | report-only |
| 28597 | Panzar'Thar Breastplate | tbc | none | 115 | Epic | chest | tank | peer_exact_ilvl_low_sample | 1 | 259.841 | unchanged |
| 24116 | Eye of the Night | tbc | none | 114 | Rare | neck | generic | peer_exact_ilvl_low_sample | 5 | 248.259 | unchanged |
| 23540 | Felsteel Longblade | tbc | none | 105 | Epic | weapon_one_hand | physical | peer_exact_ilvl_low_sample | 3 | 236.287 | unchanged |
| 29273 | Khadgar's Knapsack | tbc | none | 110 | Epic | held_in_off_hand | caster | peer_exact_ilvl_low_sample | 1 | 201.0 | unchanged |
| 25804 | Naliko's Revenge | tbc | none | 109 | Rare | finger | generic | peer_exact_ilvl | 8 | 193.477 | report-only |
| 25811 | Conqueror's Band | tbc | none | 109 | Rare | finger | generic | peer_exact_ilvl | 8 | 193.477 | report-only |
| 24250 | Bracers of Havok | tbc | none | 112 | Rare | wrist | caster | peer_exact_ilvl_low_sample | 1 | 185.536 | unchanged |
| 28168 | Insignia of the Mag'hari Hero | tbc | none | 103 | Rare | neck | generic | peer_+/-13_ilvl | 12 | 182.579 | report-only |
| 30252 | Unearthed Enkaat Wand | tbc | none | 108 | Uncommon | ranged | healer | peer_exact_ilvl_low_sample | 1 | 176.0 | unchanged |
| 22802 | Kingsfall | vanilla | none | 89 | Epic | weapon_one_hand | physical | peer_+/-10_ilvl | 8 | 172.48 | report-only |
| 821492 | SandwalkerÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â‚¬Å¡Ã‚Â¬ÃƒÂ¢Ã¢â‚¬Å¾Ã‚Â¢s Executioner | custom_or_unknown | none | 79 | Epic | weapon_two_hand | physical | peer_+/-5_ilvl | 14 | 170.71 | report-only |
| 31701 | Saboteur's Axe | tbc | none | 108 | Uncommon | weapon_two_hand | physical | peer_exact_ilvl_low_sample | 2 | 161.864 | unchanged |
| 38177 | Siege Captain's Gun | wrath | none | 146 | Uncommon | ranged | physical | peer_exact_ilvl_low_sample | 3 | 158.917 | unchanged |

## Most Underbudget Before Normalization

| entry | name | origin_label | pvp_family | item_level | quality_label | slot_key | role | target_source | target_sample_count | percent_over_under_before | action |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 12061 | Blade of Reckoning | vanilla | none | 60 | Uncommon | weapon_one_hand | physical | peer_+/-5_ilvl | 8 | -100.0 | unchanged |
| 18484 | Cho'Rush's Blade | vanilla | none | 61 | Rare | weapon_one_hand | physical | peer_+/-5_ilvl | 34 | -100.0 | unchanged |
| 13368 | Bonescraper | vanilla | none | 62 | Rare | weapon_one_hand | physical | peer_+/-5_ilvl | 32 | -100.0 | unchanged |
| 20675 | Soulrender | vanilla | none | 62 | Rare | weapon_one_hand | physical | peer_+/-5_ilvl | 32 | -100.0 | unchanged |
| 17073 | Earthshaker | vanilla | none | 66 | Epic | weapon_two_hand | physical | peer_+/-5_ilvl | 8 | -100.0 | unchanged |
| 19853 | Gurubashi Dwarf Destroyer | vanilla | none | 68 | Epic | ranged | physical | peer_+/-10_ilvl | 12 | -100.0 | unchanged |
| 19901 | Zulian Slicer | vanilla | none | 68 | Rare | weapon_one_hand | physical | peer_+/-5_ilvl | 15 | -100.0 | unchanged |
| 21478 | Bow of Taut Sinew | vanilla | none | 68 | Epic | ranged | physical | peer_+/-10_ilvl | 12 | -100.0 | unchanged |
| 17104 | Spinal Reaper | vanilla | none | 76 | Epic | weapon_two_hand | physical | peer_+/-5_ilvl | 8 | -100.0 | unchanged |
| 25327 | Frenzied Staff | tbc | none | 90 | Uncommon | weapon_two_hand | physical | peer_+/-13_ilvl | 18 | -100.0 | unchanged |
| 25300 | Lightning Dagger | tbc | none | 93 | Uncommon | weapon_main_hand | caster | peer_exact_ilvl_low_sample | 5 | -100.0 | unchanged |
| 25314 | Ceremonial Hammer | tbc | none | 93 | Uncommon | weapon_main_hand | caster | peer_exact_ilvl_low_sample | 5 | -100.0 | unchanged |
| 25328 | Faerie-Kind Staff | tbc | none | 93 | Uncommon | weapon_two_hand | physical | peer_exact_ilvl_low_sample | 1 | -100.0 | unchanged |
| 25329 | Tranquility Staff | tbc | none | 96 | Uncommon | weapon_two_hand | physical | peer_+/-6_ilvl | 12 | -100.0 | unchanged |
| 31193 | Blade of Unquenched Thirst | tbc | none | 97 | Rare | weapon_one_hand | physical | peer_exact_ilvl_low_sample | 1 | -100.0 | unchanged |
| 25330 | Starshine Staff | tbc | none | 99 | Uncommon | weapon_two_hand | physical | peer_exact_ilvl_low_sample | 4 | -100.0 | unchanged |
| 30089 | Lavaforged Warhammer | tbc | none | 100 | Rare | weapon_one_hand | physical | peer_exact_ilvl_low_sample | 3 | -100.0 | unchanged |
| 31204 | The Gunblade | tbc | none | 100 | Rare | ranged | physical | peer_+/-13_ilvl | 8 | -100.0 | unchanged |
| 31336 | Blade of Wizardry | tbc | none | 100 | Epic | weapon_main_hand | caster | peer_exact_ilvl_low_sample | 3 | -100.0 | unchanged |
| 25303 | Amplifying Blade | tbc | none | 102 | Uncommon | weapon_main_hand | caster | peer_exact_ilvl_low_sample | 2 | -100.0 | unchanged |

## Calibration Sanity Offenders

None.

## Low-Sample Calibration Groups

| expansion_band | ilvl_band | item_level | origin_label | pvp_family | quality_label | slot_key | role | count | median_percent_over_under | median_target_budget |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| burning_crusade | 120-134 | 123 | tbc | none | Epic | weapon_one_hand | generic | 2 | 326.753 | 28.39 |
| classic_endgame | 060-089 | 72 | vanilla | none | Epic | ranged | physical | 1 | 320.398 | 6.03 |
| burning_crusade | 105-119 | 113 | tbc | none | Rare | neck | generic | 1 | 290.713 | 12.06 |
| burning_crusade | 105-119 | 112 | tbc | none | Rare | cloak | generic | 2 | 274.034 | 33.545 |
| burning_crusade | 090-104 | 103 | tbc | none | Rare | neck | generic | 1 | 182.579 | 20.55 |
| classic_endgame | 060-089 | 79 | custom_or_unknown | none | Epic | weapon_two_hand | physical | 1 | 170.71 | 33.39 |
| wrath_pre_raid | 165-199 | 175 | tbc | none | Legendary | weapon_two_hand | physical | 2 | 137.831 | 277.5 |
| classic_endgame | 060-089 | 66 | vanilla | none | Rare | weapon_main_hand | caster | 1 | 130.727 | 5.5 |
| burning_crusade | 135-149 | 136 | tbc | none | Epic | weapon_one_hand | generic | 2 | 130.425 | 34.245 |
| classic_endgame | 060-089 | 83 | vanilla | none | Epic | weapon_two_hand | physical | 1 | 126.834 | 33.39 |
| wrath_t10 | 260-284 | 264 | wrath | none | Epic | weapon_main_hand | physical | 2 | 123.277 | 120.87 |
| classic_endgame | 060-089 | 84 | vanilla | none | Epic | weapon_two_hand | physical | 1 | 103.744 | 34.725 |
| classic_endgame | 060-089 | 70 | vanilla | none | Epic | weapon_two_hand | physical | 1 | 102.845 | 33.39 |
| classic_endgame | 060-089 | 71 | vanilla | none | Rare | hands | caster | 1 | 100.708 | 25.43 |
| burning_crusade | 090-104 | 90 | tbc | none | Uncommon | weapon_two_hand | physical | 1 | -100.0 | 34.55 |
| burning_crusade | 090-104 | 96 | tbc | none | Uncommon | weapon_two_hand | physical | 1 | -100.0 | 54.025 |
| burning_crusade | 090-104 | 100 | tbc | none | Rare | ranged | physical | 1 | -100.0 | 12.0 |
| burning_crusade | 090-104 | 102 | tbc | none | Uncommon | weapon_main_hand | caster | 3 | -100.0 | 13.0 |
| burning_crusade | 105-119 | 105 | tbc | none | Uncommon | weapon_main_hand | caster | 3 | -100.0 | 15.525 |
| burning_crusade | 105-119 | 108 | tbc | none | Uncommon | weapon_main_hand | caster | 3 | -100.0 | 10.86 |
| burning_crusade | 120-134 | 120 | tbc | none | Epic | weapon_one_hand | physical | 1 | -100.0 | 31.53 |
| burning_crusade | 150-164 | 151 | tbc | none | Epic | weapon_one_hand | physical | 1 | -100.0 | 42.77 |
| classic_endgame | 060-089 | 68 | vanilla | none | Rare | weapon_one_hand | physical | 1 | -100.0 | 15.0 |
| classic_endgame | 060-089 | 68 | vanilla | none | Epic | ranged | physical | 2 | -100.0 | 12.0 |
| classic_endgame | 060-089 | 76 | vanilla | none | Epic | weapon_two_hand | physical | 1 | -100.0 | 33.055 |
| burning_crusade | 105-119 | 115 | tbc | none | Epic | chest | tank | 2 | 93.816 | 101.51 |
| classic_endgame | 060-089 | 77 | vanilla | none | Epic | weapon_two_hand | physical | 1 | 84.996 | 33.39 |
| classic_endgame | 060-089 | 73 | vanilla | none | Epic | finger | tank | 1 | -83.053 | 35.405 |
| burning_crusade | 105-119 | 108 | tbc | none | Rare | weapon_one_hand | physical | 1 | 82.113 | 24.71 |
| classic_endgame | 060-089 | 70 | vanilla | none | Epic | shield | tank | 1 | -79.859 | 44.685 |
| classic_endgame | 060-089 | 71 | vanilla | none | Epic | ranged | physical | 1 | -76.071 | 14.0 |
| classic_endgame | 060-089 | 65 | vanilla | none | Rare | ranged | physical | 1 | -74.953 | 10.7 |
| burning_crusade | 090-104 | 100 | tbc | none | Epic | finger | generic | 3 | 74.642 | 33.52 |
| classic_endgame | 060-089 | 78 | vanilla | none | Epic | ranged | physical | 1 | 73.913 | 10.35 |
| burning_crusade | 105-119 | 114 | tbc | none | Rare | hands | caster | 1 | -71.294 | 73.33 |
| burning_crusade | 090-104 | 99 | tbc | none | Uncommon | weapon_one_hand | physical | 1 | 70.588 | 17.0 |
| classic_endgame | 060-089 | 88 | tbc | none | Rare | weapon_two_hand | physical | 1 | -68.187 | 77.925 |
| burning_crusade | 090-104 | 96 | tbc | none | Uncommon | weapon_one_hand | physical | 1 | -67.338 | 18.37 |
| burning_crusade | 105-119 | 110 | tbc | none | Epic | held_in_off_hand | caster | 2 | 67.111 | 28.07 |
| classic_endgame | 060-089 | 77 | vanilla | none | Epic | ranged | physical | 1 | -66.5 | 14.0 |

## SQL Blocked Rows

| entry | name | origin_label | pvp_family | item_level | quality_label | slot_key | role | target_source | target_sample_count | percent_over_under_before | sql_block_reasons | warnings |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 8289 | Arcane Leggings | vanilla | none | 60 | Uncommon | legs | caster | no_peer_skip | 0 | 0.0 | not enough comparable peer samples | normal stats unchanged: not enough comparable peer samples; armor unchanged: within +/-15% peer tolerance from 11 sample(s) |
| 8300 | Traveler's Leggings | vanilla | none | 60 | Uncommon | legs | physical | no_peer_skip | 0 | 0.0 | not enough comparable peer samples | normal stats unchanged: not enough comparable peer samples; armor unchanged: within +/-15% peer tolerance from 12 sample(s) |
| 8307 | Hero's Boots | vanilla | none | 60 | Uncommon | feet | generic | no_peer_skip | 0 | 0.0 | not enough comparable peer samples | normal stats unchanged: not enough comparable peer samples; armor unchanged: not enough comparable armor samples |
| 8310 | Hero's Pauldrons | vanilla | none | 60 | Uncommon | shoulder | healer | no_peer_skip | 0 | 0.0 | not enough comparable peer samples | normal stats unchanged: not enough comparable peer samples; armor unchanged: not enough comparable armor samples |
| 9402 | Earthborn Kilt | vanilla | none | 60 | Rare | legs | healer | peer_+/-5_ilvl | 8 | -30.889 | peer target requires 44.7% visible budget change | SQL blocked: peer target requires 44.7% visible budget change; visible stat target clamped to +/-20% safety cap; armor unchanged: within +/-15% peer tolerance from 25 sample(s) |
| 12057 | Dragonscale Band | vanilla | none | 60 | Uncommon | finger | generic | no_peer_skip | 0 | 0.0 | not enough comparable peer samples | normal stats unchanged: not enough comparable peer samples |
| 12104 | Brindlethorn Tunic | tbc | none | 60 | Uncommon | chest | physical | no_peer_skip | 0 | 0.0 | not enough comparable peer samples; origin tbc/1 did not match SQL origin filter | SQL blocked: origin tbc/1 did not match SQL origin filter; normal stats unchanged: not enough comparable peer samples; armor unchanged: within +/-15% peer tolerance from 13 sample(s) |
| 12105 | Pridemail Leggings | custom_or_unknown | none | 60 | Uncommon | legs | healer | no_peer_skip | 0 | 0.0 | not enough comparable peer samples; origin custom_or_unknown/-1 did not match SQL origin filter; special-effect/itemset rows require --allow-special-effects | spell payload detected and left unchanged; SQL blocked: origin custom_or_unknown/-1 did not match SQL origin filter; SQL blocked: special-effect/itemset rows require --allow-special-effects; normal stats unchanged: not enough comparable peer samples; armor unchanged: within +/-15% peer tolerance from 8 sample(s) |
| 12106 | Boulderskin Breastplate | tbc | none | 60 | Uncommon | chest | physical | no_peer_skip | 0 | 0.0 | not enough comparable peer samples; origin tbc/1 did not match SQL origin filter | SQL blocked: origin tbc/1 did not match SQL origin filter; normal stats unchanged: not enough comparable peer samples; armor unchanged: within +/-15% peer tolerance from 9 sample(s) |
| 12107 | Whispersilk Leggings | tbc | none | 60 | Uncommon | legs | healer | no_peer_skip | 0 | 0.0 | not enough comparable peer samples; origin tbc/1 did not match SQL origin filter | SQL blocked: origin tbc/1 did not match SQL origin filter; normal stats unchanged: not enough comparable peer samples; armor unchanged: within +/-15% peer tolerance from 11 sample(s) |
| 12185 | Bloodsail Admiral's Hat | vanilla | none | 60 | Uncommon | head | generic | no_peer_skip | 0 | 0.0 | not enough comparable peer samples; special-effect/itemset rows require --allow-special-effects | spell payload detected and left unchanged; SQL blocked: special-effect/itemset rows require --allow-special-effects; normal stats unchanged: not enough comparable peer samples; armor unchanged: within +/-15% peer tolerance from 9 sample(s) |
| 12422 | Imperial Plate Chest | vanilla | none | 60 | Uncommon | chest | physical | no_peer_skip | 0 | 0.0 | not enough comparable peer samples; special-effect/itemset rows require --allow-special-effects | itemset detected; set bonuses are not modeled; SQL blocked: special-effect/itemset rows require --allow-special-effects; normal stats unchanged: not enough comparable peer samples; armor unchanged: within +/-15% peer tolerance from 9 sample(s) |
| 12544 | Thrall's Resolve | vanilla | none | 60 | Rare | finger | physical | peer_+/-5_ilvl | 10 | -62.543 | peer target requires 167.0% visible budget change | SQL blocked: peer target requires 167.0% visible budget change; visible stat target clamped to +/-20% safety cap; armor unchanged: not enough comparable armor samples |
| 12548 | Magni's Will | vanilla | none | 60 | Rare | finger | physical | peer_+/-5_ilvl | 10 | -53.922 | special-effect/itemset rows require --allow-special-effects; peer target requires 117.0% visible budget change | spell payload detected and left unchanged; SQL blocked: special-effect/itemset rows require --allow-special-effects; SQL blocked: peer target requires 117.0% visible budget change; visible stat target clamped to +/-20% safety cap; rounding remained outside configured tolerance |
| 12604 | Starfire Tiara | vanilla | none | 60 | Rare | head | healer | peer_+/-5_ilvl | 10 | -25.071 | peer target requires 33.5% visible budget change | SQL blocked: peer target requires 33.5% visible budget change; visible stat target clamped to +/-20% safety cap; final visible stat budget adjusted to stay within +/-20% safety cap; armor unchanged: within +/-15% peer tolerance from 23 sample(s) |
| 12633 | Whitesoul Helm | vanilla | none | 60 | Rare | head | healer | no_peer_skip | 0 | 0.0 | not enough comparable peer samples | normal stats unchanged: not enough comparable peer samples; armor median from 14 sample(s) within 5 ilvl |
| 12637 | Backusarian Gauntlets | vanilla | none | 60 | Rare | hands | healer | no_peer_skip | 0 | 0.0 | not enough comparable peer samples | normal stats unchanged: not enough comparable peer samples; armor unchanged: within +/-15% peer tolerance from 17 sample(s) |
| 12952 | Gyth's Skull | vanilla | none | 60 | Rare | head | tank | no_peer_skip | 0 | 0.0 | not enough comparable peer samples | normal stats unchanged: not enough comparable peer samples; armor unchanged: within +/-15% peer tolerance from 14 sample(s) |
| 12953 | Dragoneye Coif | vanilla | none | 60 | Rare | head | physical | no_peer_skip | 0 | 0.0 | not enough comparable peer samples | normal stats unchanged: not enough comparable peer samples; armor unchanged: within +/-15% peer tolerance from 13 sample(s) |
| 12960 | Tribal War Feathers | vanilla | none | 60 | Rare | head | caster | no_peer_skip | 0 | 0.0 | not enough comparable peer samples | normal stats unchanged: not enough comparable peer samples; armor unchanged: within +/-15% peer tolerance from 17 sample(s) |
| 13072 | Stonegrip Gauntlets | vanilla | none | 60 | Rare | hands | tank | no_peer_skip | 0 | 0.0 | not enough comparable peer samples | normal stats unchanged: not enough comparable peer samples; armor unchanged: within +/-15% peer tolerance from 17 sample(s) |
| 13166 | Slamshot Shoulders | vanilla | none | 60 | Rare | shoulder | physical | peer_+/-5_ilvl | 8 | -19.498 | peer target requires 24.2% visible budget change | SQL blocked: peer target requires 24.2% visible budget change; visible stat target clamped to +/-20% safety cap; armor unchanged: within +/-15% peer tolerance from 14 sample(s) |
| 13168 | Plate of the Shaman King | vanilla | none | 60 | Rare | chest | caster | no_peer_skip | 0 | 0.0 | not enough comparable peer samples | normal stats unchanged: not enough comparable peer samples; armor unchanged: within +/-15% peer tolerance from 18 sample(s) |
| 13170 | Skyshroud Leggings | vanilla | none | 60 | Rare | legs | caster | peer_+/-5_ilvl | 9 | 37.066 | peer target requires 27.0% visible budget change | SQL blocked: peer target requires 27.0% visible budget change; visible stat target clamped to +/-20% safety cap; final visible stat budget adjusted to stay within +/-20% safety cap; armor unchanged: within +/-15% peer tolerance from 30 sample(s) |
| 13177 | Talisman of Evasion | vanilla | none | 60 | Rare | neck | tank | no_peer_skip | 0 | 0.0 | not enough comparable peer samples | normal stats unchanged: not enough comparable peer samples |
| 13179 | Brazecore Armguards | vanilla | none | 60 | Rare | wrist | healer | no_peer_skip | 0 | 0.0 | not enough comparable peer samples | normal stats unchanged: not enough comparable peer samples; armor unchanged: within +/-15% peer tolerance from 18 sample(s) |
| 13204 | Bashguuder | vanilla | none | 60 | Rare | weapon_one_hand | physical | peer_+/-5_ilvl | 35 | -30.982 | special-effect/itemset rows require --allow-special-effects; peer target requires 44.9% visible budget change | spell payload detected and left unchanged; SQL blocked: special-effect/itemset rows require --allow-special-effects; SQL blocked: peer target requires 44.9% visible budget change; visible stat target clamped to +/-20% safety cap; final visible stat budget adjusted to stay within +/-20% safety cap; weapon DPS unchanged: within +/-15% peer tolerance from 12 sample(s) |
| 13211 | Slashclaw Bracers | vanilla | none | 60 | Rare | wrist | generic | no_peer_skip | 0 | 0.0 | not enough comparable peer samples | normal stats unchanged: not enough comparable peer samples; armor unchanged: within +/-15% peer tolerance from 18 sample(s) |
| 13212 | Halycon's Spiked Collar | vanilla | none | 60 | Rare | neck | generic | no_peer_skip | 0 | 0.0 | not enough comparable peer samples; special-effect/itemset rows require --allow-special-effects | spell payload detected and left unchanged; SQL blocked: special-effect/itemset rows require --allow-special-effects; normal stats unchanged: not enough comparable peer samples |
| 13242 | Deprecated Stormrage Boots | tbc | none | 60 | Uncommon | feet | caster | no_peer_skip | 0 | 0.0 | not enough comparable peer samples; origin tbc/1 did not match SQL origin filter; special-effect/itemset rows require --allow-special-effects | spell payload detected and left unchanged; SQL blocked: origin tbc/1 did not match SQL origin filter; SQL blocked: special-effect/itemset rows require --allow-special-effects; normal stats unchanged: not enough comparable peer samples; armor unchanged: within +/-15% peer tolerance from 9 sample(s) |
| 13498 | Handcrafted Mastersmith Leggings | vanilla | none | 60 | Rare | legs | physical | no_peer_skip | 0 | 0.0 | not enough comparable peer samples | normal stats unchanged: not enough comparable peer samples; armor unchanged: within +/-15% peer tolerance from 19 sample(s) |
| 13811 | Necklace of the Dawn | custom_or_unknown | none | 60 | Uncommon | neck | generic | no_peer_skip | 0 | 0.0 | not enough comparable peer samples; origin custom_or_unknown/-1 did not match SQL origin filter | SQL blocked: origin custom_or_unknown/-1 did not match SQL origin filter; normal stats unchanged: not enough comparable peer samples |
| 13961 | Halycon's Muzzle | vanilla | none | 60 | Rare | shoulder | healer | peer_+/-10_ilvl | 10 | -40.272 | peer target requires 67.4% visible budget change | SQL blocked: peer target requires 67.4% visible budget change; visible stat target clamped to +/-20% safety cap; armor unchanged: within +/-15% peer tolerance from 21 sample(s) |
| 13962 | Vosh'gajin's Strand | vanilla | none | 60 | Rare | waist | tank | no_peer_skip | 0 | 0.0 | not enough comparable peer samples | normal stats unchanged: not enough comparable peer samples; armor unchanged: within +/-15% peer tolerance from 27 sample(s) |
| 13963 | Voone's Vice Grips | vanilla | none | 60 | Rare | hands | generic | no_peer_skip | 0 | 0.0 | not enough comparable peer samples | normal stats unchanged: not enough comparable peer samples; armor unchanged: within +/-15% peer tolerance from 23 sample(s) |
| 14458 | Elunarian Boots | vanilla | none | 60 | Uncommon | feet | healer | no_peer_skip | 0 | 0.0 | not enough comparable peer samples | normal stats unchanged: not enough comparable peer samples; armor unchanged: within +/-15% peer tolerance from 11 sample(s) |
| 14465 | Elunarian Belt | vanilla | none | 60 | Uncommon | waist | healer | no_peer_skip | 0 | 0.0 | not enough comparable peer samples | normal stats unchanged: not enough comparable peer samples; armor unchanged: within +/-15% peer tolerance from 10 sample(s) |
| 14684 | Indomitable Belt | vanilla | none | 60 | Uncommon | waist | caster | no_peer_skip | 0 | 0.0 | not enough comparable peer samples | normal stats unchanged: not enough comparable peer samples; armor unchanged: within +/-15% peer tolerance from 8 sample(s) |
| 14863 | Warleader's Gauntlets | vanilla | none | 60 | Uncommon | hands | physical | no_peer_skip | 0 | 0.0 | not enough comparable peer samples | normal stats unchanged: not enough comparable peer samples; armor unchanged: within +/-15% peer tolerance from 9 sample(s) |
| 14865 | Warleader's Greaves | vanilla | none | 60 | Uncommon | feet | healer | no_peer_skip | 0 | 0.0 | not enough comparable peer samples | normal stats unchanged: not enough comparable peer samples; armor unchanged: within +/-15% peer tolerance from 11 sample(s) |

## Weapons Changed

| entry | name | origin_label | pvp_family | item_level | quality_label | subclass_label | old_dps | new_dps | action | warnings |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 13204 | Bashguuder | vanilla | none | 60 | Rare | Mace | 39.444 | 39.444 | report-only | spell payload detected and left unchanged; SQL blocked: special-effect/itemset rows require --allow-special-effects; SQL blocked: peer target requires 44.9% visible budget change; visible stat target clamped to +/-20% safety cap; final visible stat budget adjusted to stay within +/-20% safety cap; weapon DPS unchanged: within +/-15% peer tolerance from 12 sample(s) |
| 18048 | Mastersmith's Hammer | vanilla | none | 60 | Rare | Mace | 39.375 | 39.375 | updated | special weapon stat budget excluded from normal visible stat budget; weapon damage unchanged: not enough comparable caster weapon samples |
| 18462 | Jagged Bone Fist | vanilla | none | 60 | Uncommon | Fist | 35.833 | 35.833 | report-only | SQL blocked: peer target requires 21.2% visible budget change; visible stat target clamped to +/-20% safety cap; final visible stat budget adjusted to stay within +/-20% safety cap; weapon damage unchanged: not enough comparable physical weapon samples |
| 18463 | Ogre Pocket Knife | vanilla | none | 60 | Uncommon | Sword | 35.833 | 35.833 | report-only | special weapon stat budget excluded from normal visible stat budget; SQL blocked: peer target requires 113.2% visible budget change; visible stat target clamped to +/-20% safety cap; rounding remained outside configured tolerance; weapon damage unchanged: not enough comparable physical weapon samples |
| 20654 | Amethyst War Staff | vanilla | none | 60 | Rare | Staff | 51.379 | 51.379 | report-only | special weapon stat budget excluded from normal visible stat budget; SQL blocked: peer target requires 151.5% visible budget change; visible stat target clamped to +/-20% safety cap; final visible stat budget adjusted to stay within +/-20% safety cap; weapon DPS unchanged: within +/-15% peer tolerance from 10 sample(s) |
| 20660 | Stonecutting Glaive | vanilla | none | 60 | Rare | Polearm | 51.351 | 51.351 | updated | weapon DPS unchanged: within +/-15% peer tolerance from 9 sample(s) |
| 23124 | Staff of Balzaphon | vanilla | none | 60 | Rare | Staff | 51.41 | 51.41 | updated | special weapon stat budget excluded from normal visible stat budget; weapon DPS unchanged: within +/-15% peer tolerance from 10 sample(s) |
| 13015 | Serathil | vanilla | none | 61 | Rare | Axe | 40.0 | 40.0 | report-only | SQL blocked: peer target requires 44.9% visible budget change; visible stat target clamped to +/-20% safety cap; final visible stat budget adjusted to stay within +/-20% safety cap; weapon DPS unchanged: within +/-15% peer tolerance from 12 sample(s) |
| 13380 | Willey's Portable Howitzer | vanilla | none | 61 | Rare | Gun | 31.207 | 31.207 | report-only | special weapon stat budget excluded from normal visible stat budget; SQL blocked: peer target requires 71.9% visible budget change; visible stat target clamped to +/-20% safety cap; final visible stat budget adjusted to stay within +/-20% safety cap; weapon damage unchanged: not enough comparable physical weapon samples |
| 15806 | Mirah's Song | vanilla | none | 61 | Rare | Sword | 40.0 | 40.0 | report-only | SQL blocked: peer target requires 36.0% visible budget change; visible stat target clamped to +/-20% safety cap; final visible stat budget adjusted to stay within +/-20% safety cap; weapon DPS unchanged: within +/-15% peer tolerance from 11 sample(s) |
| 18347 | Well Balanced Axe | vanilla | none | 61 | Uncommon | Axe | 36.316 | 36.316 | updated | weapon damage unchanged: not enough comparable physical weapon samples |
| 18680 | Ancient Bone Bow | vanilla | none | 61 | Rare | Bow | 31.25 | 31.25 | updated | rounding remained outside configured tolerance; weapon DPS unchanged: within +/-15% peer tolerance from 9 sample(s) |
| 18683 | Hammer of the Vesper | vanilla | none | 61 | Rare | Mace | 40.2 | 40.2 | report-only | SQL blocked: peer target requires 23.4% visible budget change; visible stat target clamped to +/-20% safety cap; final visible stat budget adjusted to stay within +/-20% safety cap; weapon DPS unchanged: within +/-15% peer tolerance from 12 sample(s) |
| 22317 | Lefty's Brass Knuckle | vanilla | none | 61 | Rare | Fist | 40.0 | 40.0 | report-only | SQL blocked: peer target requires 32.2% visible budget change; visible stat target clamped to +/-20% safety cap; weapon damage unchanged: not enough comparable physical weapon samples |
| 22322 | The Jaw Breaker | vanilla | none | 61 | Rare | Mace | 40.312 | 40.312 | updated | weapon DPS unchanged: within +/-15% peer tolerance from 12 sample(s) |
| 13006 | Mass of McGowan | vanilla | none | 62 | Rare | Mace | 40.893 | 40.893 | report-only | SQL blocked: peer target requires 21.9% visible budget change; visible stat target clamped to +/-20% safety cap; weapon DPS unchanged: within +/-15% peer tolerance from 11 sample(s) |
| 13964 | Witchblade | vanilla | none | 62 | Rare | Dagger | 40.625 | 40.625 | report-only | special weapon stat budget excluded from normal visible stat budget; SQL blocked: peer target requires 31.2% visible budget change; visible stat target clamped to +/-20% safety cap; rounding remained outside configured tolerance; final visible stat budget adjusted to stay within +/-20% safety cap; weapon damage unchanged: not enough comparable caster weapon samples |
| 16996 | Gorewood Bow | vanilla | none | 62 | Rare | Bow | 31.8 | 31.8 | updated | weapon DPS unchanged: within +/-15% peer tolerance from 9 sample(s) |
| 18396 | Mind Carver | vanilla | none | 62 | Rare | Sword | 40.75 | 40.75 | report-only | special weapon stat budget excluded from normal visible stat budget; SQL blocked: peer target requires 31.2% visible budget change; visible stat target clamped to +/-20% safety cap; rounding remained outside configured tolerance; final visible stat budget adjusted to stay within +/-20% safety cap; weapon damage unchanged: not enough comparable caster weapon samples |
| 18737 | Bone Slicing Hatchet | vanilla | none | 62 | Rare | Axe | 40.588 | 40.588 | report-only | SQL blocked: peer target requires 20.2% visible budget change; visible stat target clamped to +/-20% safety cap; rounding remained outside configured tolerance; final visible stat budget adjusted to stay within +/-20% safety cap; weapon DPS unchanged: within +/-15% peer tolerance from 12 sample(s) |
| 18755 | Xorothian Firestick | vanilla | none | 62 | Rare | Gun | 31.731 | 31.731 | report-only | SQL blocked: peer target requires 29.2% visible budget change; visible stat target clamped to +/-20% safety cap; final visible stat budget adjusted to stay within +/-20% safety cap; weapon damage unchanged: not enough comparable physical weapon samples |
| 18759 | Malicious Axe | vanilla | none | 62 | Rare | Two-hand Axe | 52.903 | 52.903 | report-only | special weapon stat budget excluded from normal visible stat budget; SQL blocked: peer target requires 84.3% visible budget change; visible stat target clamped to +/-20% safety cap; weapon DPS unchanged: within +/-15% peer tolerance from 12 sample(s) |
| 20666 | Hardened Steel Warhammer | vanilla | none | 62 | Rare | Mace | 40.769 | 40.769 | report-only | special weapon stat budget excluded from normal visible stat budget; SQL blocked: peer target requires 56.7% visible budget change; visible stat target clamped to +/-20% safety cap; weapon damage unchanged: not enough comparable caster weapon samples |
| 20669 | Darkstone Claymore | vanilla | none | 62 | Rare | Two-hand Sword | 52.917 | 52.917 | report-only | special weapon stat budget excluded from normal visible stat budget; SQL blocked: peer target requires 225.2% visible budget change; visible stat target clamped to +/-20% safety cap; weapon DPS unchanged: within +/-15% peer tolerance from 16 sample(s) |
| 22332 | Blade of Necromancy | vanilla | none | 62 | Rare | Sword | 40.667 | 40.667 | report-only | SQL blocked: peer target requires 29.9% visible budget change; visible stat target clamped to +/-20% safety cap; final visible stat budget adjusted to stay within +/-20% safety cap; weapon DPS unchanged: within +/-15% peer tolerance from 11 sample(s) |
| 22333 | Hammer of Divine Might | vanilla | none | 62 | Rare | Two-hand Mace | 53.095 | 53.095 | report-only | special weapon stat budget excluded from normal visible stat budget; SQL blocked: peer target requires 38.1% visible budget change; visible stat target clamped to +/-20% safety cap; final visible stat budget adjusted to stay within +/-20% safety cap; weapon damage unchanged: not enough comparable caster weapon samples |
| 23132 | Lord Blackwood's Blade | vanilla | none | 62 | Rare | Sword | 40.667 | 40.667 | report-only | SQL blocked: peer target requires 49.3% visible budget change; visible stat target clamped to +/-20% safety cap; final visible stat budget adjusted to stay within +/-20% safety cap; weapon DPS unchanged: within +/-15% peer tolerance from 11 sample(s) |
| 5267 | Scarlet Kris | vanilla | none | 63 | Rare | Dagger | 41.333 | 41.333 | report-only | SQL blocked: peer target requires 31.0% visible budget change; visible stat target clamped to +/-20% safety cap; weapon DPS unchanged: within +/-15% peer tolerance from 10 sample(s) |
| 12783 | Heartseeker | vanilla | none | 63 | Rare | Dagger | 41.471 | 41.471 | report-only | SQL blocked: peer target requires 35.1% visible budget change; visible stat target clamped to +/-20% safety cap; final visible stat budget adjusted to stay within +/-20% safety cap; weapon DPS unchanged: within +/-15% peer tolerance from 10 sample(s) |
| 12784 | Arcanite Reaper | vanilla | none | 63 | Rare | Two-hand Axe | 53.816 | 53.816 | report-only | special weapon stat budget excluded from normal visible stat budget; SQL blocked: peer target requires 325.3% visible budget change; visible stat target clamped to +/-20% safety cap; final visible stat budget adjusted to stay within +/-20% safety cap; weapon DPS unchanged: within +/-15% peer tolerance from 8 sample(s) |

## Armor Changed

| entry | name | origin_label | pvp_family | item_level | quality_label | subclass_label | old_armor | new_armor | action | warnings |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 12633 | Whitesoul Helm | vanilla | none | 60 | Rare | Plate | 629 | 530 | report-only | normal stats unchanged: not enough comparable peer samples; armor median from 14 sample(s) within 5 ilvl |
| 13252 | Cloudrunner Girdle | vanilla | none | 60 | Rare | Leather | 185 | 97 | updated | normal stats unchanged: within +/-15% peer tolerance; armor median from 27 sample(s) within 5 ilvl |
| 17013 | Dark Iron Leggings | vanilla | none | 60 | Epic | Plate | 778 | 660 | report-only | normal stats unchanged: not enough comparable peer samples; armor median from 10 sample(s) within 10 ilvl |
| 20693 | Weighted Cloak | vanilla | none | 60 | Uncommon | Cloth | 99 | 37 | report-only | normal stats unchanged: not enough comparable peer samples; armor median from 21 sample(s) within 5 ilvl |
| 22385 | Titanic Leggings | vanilla | none | 60 | Epic | Plate | 598 | 706 | report-only | normal stats unchanged: not enough comparable peer samples; armor median from 10 sample(s) within 10 ilvl |
| 13955 | Stoneform Shoulders | vanilla | none | 61 | Rare | Plate | 688 | 489 | report-only | normal stats unchanged: not enough comparable peer samples; armor median from 14 sample(s) within 5 ilvl |
| 16980 | Flarecore Mantle | vanilla | none | 61 | Epic | Cloth | 71 | 87 | report-only | normal stats unchanged: not enough comparable peer samples; armor median from 8 sample(s) within 10 ilvl |
| 16984 | Black Dragonscale Boots | vanilla | none | 61 | Epic | Mail | 270 | 331 | report-only | itemset detected; set bonuses are not modeled; SQL blocked: special-effect/itemset rows require --allow-special-effects; normal stats unchanged: not enough comparable peer samples; armor median from 10 sample(s) within 10 ilvl |
| 18351 | Magically Sealed Bracers | vanilla | none | 61 | Uncommon | Plate | 383 | 243 | report-only | normal stats unchanged: not enough comparable peer samples; armor median from 10 sample(s) within 10 ilvl |
| 18383 | Force Imbued Gauntlets | vanilla | none | 61 | Rare | Plate | 538 | 404 | report-only | normal stats unchanged: not enough comparable peer samples; armor median from 21 sample(s) within 5 ilvl |
| 18861 | Flamewaker Legplates | vanilla | none | 61 | Epic | Plate | 748 | 646 | report-only | normal stats unchanged: not enough comparable peer samples; armor median from 9 sample(s) within 5 ilvl |
| 19835 | Zandalar Madcap's Mantle | vanilla | none | 61 | Epic | Leather | 140 | 225 | report-only | itemset detected; set bonuses are not modeled; SQL blocked: special-effect/itemset rows require --allow-special-effects; normal stats unchanged: not enough comparable peer samples; armor median from 8 sample(s) within 10 ilvl |
| 19839 | Zandalar Haruspex's Belt | vanilla | none | 61 | Epic | Leather | 165 | 111 | report-only | itemset detected; set bonuses are not modeled; SQL blocked: special-effect/itemset rows require --allow-special-effects; normal stats unchanged: not enough comparable peer samples; armor median from 8 sample(s) within 5 ilvl |
| 19840 | Zandalar Haruspex's Bracers | vanilla | none | 61 | Epic | Leather | 122 | 86 | report-only | itemset detected; set bonuses are not modeled; SQL blocked: special-effect/itemset rows require --allow-special-effects; normal stats unchanged: not enough comparable peer samples; armor median from 10 sample(s) within 5 ilvl |
| 19845 | Zandalar Illusionist's Mantle | vanilla | none | 61 | Epic | Cloth | 71 | 87 | report-only | itemset detected; set bonuses are not modeled; SQL blocked: special-effect/itemset rows require --allow-special-effects; normal stats unchanged: not enough comparable peer samples; armor median from 8 sample(s) within 10 ilvl |
| 19849 | Zandalar Demoniac's Mantle | vanilla | none | 61 | Epic | Cloth | 71 | 87 | report-only | itemset detected; set bonuses are not modeled; SQL blocked: special-effect/itemset rows require --allow-special-effects; normal stats unchanged: not enough comparable peer samples; armor median from 8 sample(s) within 10 ilvl |
| 22304 | Ironweave Gloves | vanilla | none | 61 | Rare | Cloth | 144 | 55 | report-only | itemset detected; set bonuses are not modeled; SQL blocked: special-effect/itemset rows require --allow-special-effects; normal stats unchanged: within +/-15% peer tolerance; armor median from 31 sample(s) within 5 ilvl |
| 22305 | Ironweave Mantle | vanilla | none | 61 | Rare | Cloth | 155 | 67 | report-only | itemset detected; set bonuses are not modeled; SQL blocked: special-effect/itemset rows require --allow-special-effects; SQL blocked: peer target requires 61.0% visible budget change; visible stat target clamped to +/-20% safety cap; armor median from 27 sample(s) within 5 ilvl |
| 22306 | Ironweave Belt | vanilla | none | 61 | Rare | Cloth | 139 | 48 | report-only | itemset detected; set bonuses are not modeled; SQL blocked: special-effect/itemset rows require --allow-special-effects; SQL blocked: peer target requires 47.1% visible budget change; visible stat target clamped to +/-20% safety cap; armor median from 25 sample(s) within 5 ilvl |
| 22311 | Ironweave Boots | vanilla | none | 61 | Rare | Cloth | 150 | 61 | report-only | itemset detected; set bonuses are not modeled; SQL blocked: special-effect/itemset rows require --allow-special-effects; SQL blocked: peer target requires 61.0% visible budget change; visible stat target clamped to +/-20% safety cap; armor median from 34 sample(s) within 5 ilvl |
| 22313 | Ironweave Bracers | vanilla | none | 61 | Rare | Cloth | 108 | 37 | report-only | itemset detected; set bonuses are not modeled; SQL blocked: special-effect/itemset rows require --allow-special-effects; normal stats unchanged: not enough comparable peer samples; armor median from 20 sample(s) within 5 ilvl |
| 12756 | Leggings of Arcana | vanilla | none | 62 | Epic | Leather | 166 | 242 | report-only | normal stats unchanged: not enough comparable peer samples; armor median from 12 sample(s) within 10 ilvl |
| 12903 | Legguards of the Chromatic Defier | vanilla | none | 62 | Epic | Mail | 349 | 415 | report-only | normal stats unchanged: not enough comparable peer samples; armor median from 13 sample(s) within 10 ilvl |
| 12945 | Legplates of the Chromatic Defier | vanilla | none | 62 | Epic | Mail | 349 | 415 | report-only | normal stats unchanged: not enough comparable peer samples; armor median from 13 sample(s) within 10 ilvl |
| 13936 | Dreadmaster's Shroud | tbc | none | 62 | Rare | Cloth | 120 | 73 | report-only | SQL blocked: origin tbc/1 did not match SQL origin filter; normal stats unchanged: not enough comparable peer samples; armor median from 21 sample(s) within 5 ilvl |
| 14146 | Gloves of Spell Mastery | vanilla | none | 62 | Epic | Cloth | 60 | 72 | report-only | SQL blocked: peer target requires 25.8% visible budget change; visible stat target clamped to +/-20% safety cap; final visible stat budget adjusted to stay within +/-20% safety cap; armor median from 17 sample(s) within 10 ilvl |
| 14554 | Cloudkeeper Legplates | vanilla | none | 62 | Epic | Plate | 617 | 737 | report-only | spell payload detected and left unchanged; SQL blocked: special-effect/itemset rows require --allow-special-effects; normal stats unchanged: not enough comparable peer samples; armor median from 9 sample(s) within 5 ilvl |
| 15141 | Onyxia Scale Breastplate | wrath | none | 62 | Epic | Mail | 605 | 416 | report-only | spell payload detected and left unchanged; SQL blocked: origin wrath/2 did not match SQL origin filter; SQL blocked: special-effect/itemset rows require --allow-special-effects; normal stats unchanged: not enough comparable peer samples; armor median from 9 sample(s) within 10 ilvl |
| 16979 | Flarecore Gloves | vanilla | none | 62 | Epic | Cloth | 60 | 72 | report-only | SQL blocked: peer target requires 122.3% visible budget change; visible stat target clamped to +/-20% safety cap; final visible stat budget adjusted to stay within +/-20% safety cap; armor median from 17 sample(s) within 10 ilvl |
| 16988 | Fiery Chain Shoulders | vanilla | none | 62 | Epic | Mail | 299 | 356 | report-only | normal stats unchanged: not enough comparable peer samples; armor median from 9 sample(s) within 10 ilvl |

## Items With Sockets

| entry | name | origin_label | item_level | quality_label | slot_key | socket_budget | action | warnings |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 27830 | Circlet of the Victor | tbc | 80 | Rare | finger | 8.0 | unchanged | socket budget counted; socket colors and socket bonus unchanged; SQL blocked: origin tbc/1 did not match SQL origin filter; normal stats unchanged: not enough comparable peer samples |
| 27832 | Band of the Victor | tbc | 80 | Rare | finger | 8.0 | unchanged | socket budget counted; socket colors and socket bonus unchanged; SQL blocked: origin tbc/1 did not match SQL origin filter; normal stats unchanged: not enough comparable peer samples |
| 27833 | Band of the Victor | tbc | 80 | Rare | finger | 8.0 | unchanged | socket budget counted; socket colors and socket bonus unchanged; SQL blocked: origin tbc/1 did not match SQL origin filter; normal stats unchanged: not enough comparable peer samples |
| 27834 | Circlet of the Victor | tbc | 80 | Rare | finger | 8.0 | unchanged | socket budget counted; socket colors and socket bonus unchanged; SQL blocked: origin tbc/1 did not match SQL origin filter; normal stats unchanged: not enough comparable peer samples |
| 24021 | Light-Touched Breastplate | tbc | 85 | Rare | chest | 24.0 | unchanged | socket budget counted; socket colors and socket bonus unchanged; SQL blocked: origin tbc/1 did not match SQL origin filter; normal stats unchanged: not enough comparable peer samples; armor unchanged: not enough comparable armor samples |
| 24022 | Scale Leggings of the Skirmisher | tbc | 85 | Rare | legs | 24.0 | unchanged | socket budget counted; socket colors and socket bonus unchanged; SQL blocked: origin tbc/1 did not match SQL origin filter; normal stats unchanged: not enough comparable peer samples; armor unchanged: not enough comparable armor samples |
| 24046 | Kilt of Rolling Thunders | tbc | 85 | Rare | legs | 24.0 | unchanged | socket budget counted; socket colors and socket bonus unchanged; SQL blocked: origin tbc/1 did not match SQL origin filter; normal stats unchanged: not enough comparable peer samples; armor unchanged: not enough comparable armor samples |
| 24063 | Shifting Sash of Midnight | tbc | 85 | Rare | waist | 16.0 | unchanged | socket budget counted; socket colors and socket bonus unchanged; SQL blocked: origin tbc/1 did not match SQL origin filter; normal stats unchanged: not enough comparable peer samples; armor unchanged: not enough comparable armor samples |
| 24064 | Ironsole Clompers | tbc | 85 | Rare | feet | 16.0 | unchanged | socket budget counted; socket colors and socket bonus unchanged; SQL blocked: origin tbc/1 did not match SQL origin filter; normal stats unchanged: not enough comparable peer samples; armor unchanged: not enough comparable armor samples |
| 24083 | Lifegiver Britches | tbc | 85 | Rare | legs | 24.0 | unchanged | socket budget counted; socket colors and socket bonus unchanged; SQL blocked: origin tbc/1 did not match SQL origin filter; normal stats unchanged: not enough comparable peer samples; armor unchanged: not enough comparable armor samples |
| 24090 | Bloodstained Ravager Gauntlets | tbc | 85 | Rare | hands | 16.0 | unchanged | socket budget counted; socket colors and socket bonus unchanged; SQL blocked: origin tbc/1 did not match SQL origin filter; normal stats unchanged: not enough comparable peer samples; armor unchanged: not enough comparable armor samples |
| 24091 | Tenacious Defender | tbc | 85 | Rare | waist | 16.0 | unchanged | socket budget counted; socket colors and socket bonus unchanged; SQL blocked: origin tbc/1 did not match SQL origin filter; normal stats unchanged: not enough comparable peer samples; armor unchanged: not enough comparable armor samples |
| 153959 | Lightdrinker Dagger | custom_or_unknown | 85 | Rare | weapon_one_hand | 8.0 | unchanged | special weapon stat budget excluded from normal visible stat budget; socket budget counted; socket colors and socket bonus unchanged; SQL blocked: origin custom_or_unknown/-1 did not match SQL origin filter; normal stats unchanged: not enough comparable peer samples; weapon damage unchanged: not enough comparable physical weapon samples |
| 24387 | Ironblade Gauntlets | tbc | 88 | Rare | hands | 16.0 | unchanged | socket budget counted; socket colors and socket bonus unchanged; SQL blocked: origin tbc/1 did not match SQL origin filter; normal stats unchanged: not enough comparable peer samples; armor unchanged: not enough comparable armor samples |
| 24388 | Girdle of the Gale Storm | tbc | 88 | Rare | waist | 16.0 | unchanged | socket budget counted; socket colors and socket bonus unchanged; SQL blocked: origin tbc/1 did not match SQL origin filter; normal stats unchanged: not enough comparable peer samples; armor unchanged: not enough comparable armor samples |
| 24391 | Kilt of the Night Strider | tbc | 88 | Rare | legs | 24.0 | unchanged | socket budget counted; socket colors and socket bonus unchanged; SQL blocked: origin tbc/1 did not match SQL origin filter; normal stats unchanged: not enough comparable peer samples; armor unchanged: not enough comparable armor samples |
| 24393 | Bloody Surgeon's Mitts | tbc | 88 | Rare | hands | 16.0 | unchanged | socket budget counted; socket colors and socket bonus unchanged; SQL blocked: origin tbc/1 did not match SQL origin filter; normal stats unchanged: not enough comparable peer samples; armor unchanged: not enough comparable armor samples |
| 24395 | Mindfire Waistband | tbc | 88 | Rare | waist | 16.0 | unchanged | socket budget counted; socket colors and socket bonus unchanged; SQL blocked: origin tbc/1 did not match SQL origin filter; normal stats unchanged: not enough comparable peer samples; armor unchanged: not enough comparable armor samples |
| 24396 | Vest of Vengeance | tbc | 88 | Rare | chest | 24.0 | unchanged | socket budget counted; socket colors and socket bonus unchanged; SQL blocked: origin tbc/1 did not match SQL origin filter; normal stats unchanged: not enough comparable peer samples; armor unchanged: not enough comparable armor samples |
| 24397 | Raiments of Divine Authority | tbc | 88 | Rare | chest | 24.0 | unchanged | socket budget counted; socket colors and socket bonus unchanged; SQL blocked: origin tbc/1 did not match SQL origin filter; normal stats unchanged: not enough comparable peer samples; armor unchanged: not enough comparable armor samples |
| 23362 | Hammer of the Sun | custom_or_unknown | 90 | Epic | weapon_main_hand | 28.0 | unchanged | socket budget counted; socket colors and socket bonus unchanged; SQL blocked: origin custom_or_unknown/-1 did not match SQL origin filter; normal stats unchanged: not enough comparable peer samples |
| 23363 | Titanic Breastplate | custom_or_unknown | 90 | Epic | chest | 36.0 | unchanged | socket budget counted; socket colors and socket bonus unchanged; SQL blocked: origin custom_or_unknown/-1 did not match SQL origin filter; normal stats unchanged: not enough comparable peer samples |
| 27715 | Circle's Stalwart Helmet | tbc | 90 | Uncommon | head | 24.0 | unchanged | socket budget counted; socket colors and socket bonus unchanged; SQL blocked: origin tbc/1 did not match SQL origin filter; normal stats unchanged: not enough comparable peer samples; armor unchanged: not enough comparable armor samples |
| 27717 | Expedition Forager Leggings | tbc | 90 | Uncommon | legs | 24.0 | unchanged | socket budget counted; socket colors and socket bonus unchanged; SQL blocked: origin tbc/1 did not match SQL origin filter; normal stats unchanged: not enough comparable peer samples; armor unchanged: not enough comparable armor samples |
| 31657 | Chemise of Rebirth | tbc | 90 | Uncommon | chest | 24.0 | unchanged | socket budget counted; socket colors and socket bonus unchanged; SQL blocked: origin tbc/1 did not match SQL origin filter; normal stats unchanged: not enough comparable peer samples; armor unchanged: not enough comparable armor samples |
| 31658 | Scout's Hood | tbc | 90 | Uncommon | head | 24.0 | unchanged | socket budget counted; socket colors and socket bonus unchanged; SQL blocked: origin tbc/1 did not match SQL origin filter; normal stats unchanged: not enough comparable peer samples; armor unchanged: within +/-15% peer tolerance from 9 sample(s) |
| 24357 | Vest of Living Lightning | tbc | 91 | Rare | chest | 24.0 | unchanged | socket budget counted; socket colors and socket bonus unchanged; SQL blocked: origin tbc/1 did not match SQL origin filter; normal stats unchanged: not enough comparable peer samples; armor unchanged: not enough comparable armor samples |
| 24363 | Unscarred Breastplate | tbc | 91 | Rare | chest | 24.0 | unchanged | socket budget counted; socket colors and socket bonus unchanged; SQL blocked: origin tbc/1 did not match SQL origin filter; normal stats unchanged: not enough comparable peer samples; armor unchanged: within +/-15% peer tolerance from 11 sample(s) |
| 24365 | Deft Handguards | tbc | 91 | Rare | hands | 16.0 | unchanged | socket budget counted; socket colors and socket bonus unchanged; SQL blocked: origin tbc/1 did not match SQL origin filter; normal stats unchanged: not enough comparable peer samples; armor unchanged: not enough comparable armor samples |
| 24366 | Scorpid-Sting Mantle | tbc | 91 | Rare | shoulder | 16.0 | unchanged | socket budget counted; socket colors and socket bonus unchanged; SQL blocked: origin tbc/1 did not match SQL origin filter; SQL blocked: exact peer sample count is below configured minimum; armor unchanged: not enough comparable armor samples |

## Spell Payloads Detected

| entry | name | origin_label | item_level | quality_label | slot_key | action | warnings |
| --- | --- | --- | --- | --- | --- | --- | --- |
| 11810 | Force of Will | vanilla | 60 | Rare |  | skipped | spell payload detected and left unchanged; SQL blocked: special-effect/itemset rows require --allow-special-effects; normal stats unchanged: item could not be peer-classified |
| 12105 | Pridemail Leggings | custom_or_unknown | 60 | Uncommon | legs | unchanged | spell payload detected and left unchanged; SQL blocked: origin custom_or_unknown/-1 did not match SQL origin filter; SQL blocked: special-effect/itemset rows require --allow-special-effects; normal stats unchanged: not enough comparable peer samples; armor unchanged: within +/-15% peer tolerance from 8 sample(s) |
| 12185 | Bloodsail Admiral's Hat | vanilla | 60 | Uncommon | head | unchanged | spell payload detected and left unchanged; SQL blocked: special-effect/itemset rows require --allow-special-effects; normal stats unchanged: not enough comparable peer samples; armor unchanged: within +/-15% peer tolerance from 9 sample(s) |
| 12548 | Magni's Will | vanilla | 60 | Rare | finger | report-only | spell payload detected and left unchanged; SQL blocked: special-effect/itemset rows require --allow-special-effects; SQL blocked: peer target requires 117.0% visible budget change; visible stat target clamped to +/-20% safety cap; rounding remained outside configured tolerance |
| 13204 | Bashguuder | vanilla | 60 | Rare | weapon_one_hand | report-only | spell payload detected and left unchanged; SQL blocked: special-effect/itemset rows require --allow-special-effects; SQL blocked: peer target requires 44.9% visible budget change; visible stat target clamped to +/-20% safety cap; final visible stat budget adjusted to stay within +/-20% safety cap; weapon DPS unchanged: within +/-15% peer tolerance from 12 sample(s) |
| 13212 | Halycon's Spiked Collar | vanilla | 60 | Rare | neck | unchanged | spell payload detected and left unchanged; SQL blocked: special-effect/itemset rows require --allow-special-effects; normal stats unchanged: not enough comparable peer samples |
| 13242 | Deprecated Stormrage Boots | tbc | 60 | Uncommon | feet | unchanged | spell payload detected and left unchanged; SQL blocked: origin tbc/1 did not match SQL origin filter; SQL blocked: special-effect/itemset rows require --allow-special-effects; normal stats unchanged: not enough comparable peer samples; armor unchanged: within +/-15% peer tolerance from 9 sample(s) |
| 13603 | Major Spellstone (DEPRECATED) | vanilla | 60 | 1 | relic | skipped | spell payload detected and left unchanged; SQL blocked: special-effect/itemset rows require --allow-special-effects; normal stats unchanged: not enough comparable peer samples |
| 15138 | Onyxia Scale Cloak | vanilla | 60 | Rare | cloak | report-only | spell payload detected and left unchanged; SQL blocked: special-effect/itemset rows require --allow-special-effects; SQL blocked: peer target requires 100.0% visible budget change; visible stat target clamped to +/-20% safety cap; rounding remained outside configured tolerance; armor unchanged: within +/-15% peer tolerance from 42 sample(s) |
| 17901 | Stormpike Insignia Rank 3 | vanilla | 60 | Uncommon |  | skipped | spell payload detected and left unchanged; SQL blocked: special-effect/itemset rows require --allow-special-effects; normal stats unchanged: item could not be peer-classified |
| 17902 | Stormpike Insignia Rank 4 | vanilla | 60 | Rare |  | skipped | spell payload detected and left unchanged; SQL blocked: special-effect/itemset rows require --allow-special-effects; normal stats unchanged: item could not be peer-classified |
| 17903 | Stormpike Insignia Rank 5 | vanilla | 60 | Rare |  | skipped | spell payload detected and left unchanged; SQL blocked: special-effect/itemset rows require --allow-special-effects; normal stats unchanged: item could not be peer-classified |
| 17904 | Stormpike Insignia Rank 6 | vanilla | 60 | Epic |  | skipped | spell payload detected and left unchanged; SQL blocked: special-effect/itemset rows require --allow-special-effects; normal stats unchanged: item could not be peer-classified |
| 17906 | Frostwolf Insignia Rank 3 | vanilla | 60 | Uncommon |  | skipped | spell payload detected and left unchanged; SQL blocked: special-effect/itemset rows require --allow-special-effects; normal stats unchanged: item could not be peer-classified |
| 17907 | Frostwolf Insignia Rank 4 | vanilla | 60 | Rare |  | skipped | spell payload detected and left unchanged; SQL blocked: special-effect/itemset rows require --allow-special-effects; normal stats unchanged: item could not be peer-classified |
| 17908 | Frostwolf Insignia Rank 5 | vanilla | 60 | Rare |  | skipped | spell payload detected and left unchanged; SQL blocked: special-effect/itemset rows require --allow-special-effects; normal stats unchanged: item could not be peer-classified |
| 17909 | Frostwolf Insignia Rank 6 | vanilla | 60 | Epic |  | skipped | spell payload detected and left unchanged; SQL blocked: special-effect/itemset rows require --allow-special-effects; normal stats unchanged: item could not be peer-classified |
| 18345 | Murmuring Ring | vanilla | 60 | Uncommon | finger | unchanged | spell payload detected and left unchanged; SQL blocked: special-effect/itemset rows require --allow-special-effects; normal stats unchanged: not enough comparable peer samples |
| 18346 | Threadbare Trousers | vanilla | 60 | Uncommon | legs | unchanged | spell payload detected and left unchanged; SQL blocked: special-effect/itemset rows require --allow-special-effects; normal stats unchanged: not enough comparable peer samples; armor unchanged: within +/-15% peer tolerance from 11 sample(s) |
| 18387 | Brightspark Gloves | vanilla | 60 | Rare | hands | report-only | spell payload detected and left unchanged; SQL blocked: special-effect/itemset rows require --allow-special-effects; SQL blocked: peer target requires 20.9% visible budget change; visible stat target clamped to +/-20% safety cap; final visible stat budget adjusted to stay within +/-20% safety cap; armor unchanged: within +/-15% peer tolerance from 26 sample(s) |
| 18450 | Robe of Combustion | vanilla | 60 | Uncommon | chest | unchanged | spell payload detected and left unchanged; SQL blocked: special-effect/itemset rows require --allow-special-effects; normal stats unchanged: not enough comparable peer samples; armor unchanged: within +/-15% peer tolerance from 8 sample(s) |
| 18602 | Tome of Sacrifice | vanilla | 60 | Rare | held_in_off_hand | unchanged | spell payload detected and left unchanged; SQL blocked: special-effect/itemset rows require --allow-special-effects; normal stats unchanged: not enough comparable peer samples |
| 18728 | Anastari Heirloom | vanilla | 60 | Rare | neck | unchanged | spell payload detected and left unchanged; SQL blocked: special-effect/itemset rows require --allow-special-effects; normal stats unchanged: not enough comparable peer samples |
| 21784 | Figurine - Black Diamond Crab | tbc | 60 | Rare |  | skipped | spell payload detected and left unchanged; SQL blocked: origin tbc/1 did not match SQL origin filter; SQL blocked: special-effect/itemset rows require --allow-special-effects; normal stats unchanged: item could not be peer-classified |
| 21789 | Figurine - Dark Iron Scorpid | tbc | 60 | Rare |  | skipped | spell payload detected and left unchanged; SQL blocked: origin tbc/1 did not match SQL origin filter; SQL blocked: special-effect/itemset rows require --allow-special-effects; normal stats unchanged: item could not be peer-classified |
| 22003 | Darkmantle Boots | vanilla | 60 | Epic | feet | unchanged | spell payload detected and left unchanged; itemset detected; set bonuses are not modeled; SQL blocked: special-effect/itemset rows require --allow-special-effects; normal stats unchanged: not enough comparable peer samples; armor unchanged: not enough comparable armor samples |
| 22013 | Beastmaster's Cap | vanilla | 60 | Epic | head | unchanged | spell payload detected and left unchanged; itemset detected; set bonuses are not modeled; SQL blocked: special-effect/itemset rows require --allow-special-effects; normal stats unchanged: not enough comparable peer samples; armor unchanged: not enough comparable armor samples |
| 22060 | Beastmaster's Tunic | vanilla | 60 | Epic | chest | unchanged | spell payload detected and left unchanged; itemset detected; set bonuses are not modeled; SQL blocked: special-effect/itemset rows require --allow-special-effects; normal stats unchanged: not enough comparable peer samples; armor unchanged: within +/-15% peer tolerance from 8 sample(s) |
| 22061 | Beastmaster's Boots | vanilla | 60 | Epic | feet | unchanged | spell payload detected and left unchanged; itemset detected; set bonuses are not modeled; SQL blocked: special-effect/itemset rows require --allow-special-effects; normal stats unchanged: not enough comparable peer samples; armor unchanged: not enough comparable armor samples |
| 22069 | Sorcerer's Robes | vanilla | 60 | Epic | chest | report-only | spell payload detected and left unchanged; itemset detected; set bonuses are not modeled; SQL blocked: special-effect/itemset rows require --allow-special-effects; final visible stat budget adjusted to stay within +/-20% safety cap; armor unchanged: within +/-15% peer tolerance from 10 sample(s) |

## Itemsets Detected

| entry | name | origin_label | item_level | quality_label | slot_key | action | warnings |
| --- | --- | --- | --- | --- | --- | --- | --- |
| 12422 | Imperial Plate Chest | vanilla | 60 | Uncommon | chest | unchanged | itemset detected; set bonuses are not modeled; SQL blocked: special-effect/itemset rows require --allow-special-effects; normal stats unchanged: not enough comparable peer samples; armor unchanged: within +/-15% peer tolerance from 9 sample(s) |
| 15051 | Black Dragonscale Shoulders | vanilla | 60 | Rare | shoulder | unchanged | itemset detected; set bonuses are not modeled; SQL blocked: special-effect/itemset rows require --allow-special-effects; normal stats unchanged: not enough comparable peer samples; armor unchanged: within +/-15% peer tolerance from 19 sample(s) |
| 15062 | Devilsaur Leggings | vanilla | 60 | Rare | legs | unchanged | itemset detected; set bonuses are not modeled; SQL blocked: special-effect/itemset rows require --allow-special-effects; normal stats unchanged: not enough comparable peer samples; armor unchanged: within +/-15% peer tolerance from 25 sample(s) |
| 16669 | Pauldrons of Elements | vanilla | 60 | Rare | shoulder | unchanged | itemset detected; set bonuses are not modeled; SQL blocked: special-effect/itemset rows require --allow-special-effects; normal stats unchanged: within +/-15% peer tolerance; armor unchanged: within +/-15% peer tolerance from 19 sample(s) |
| 16679 | Beaststalker's Mantle | vanilla | 60 | Rare | shoulder | unchanged | itemset detected; set bonuses are not modeled; SQL blocked: special-effect/itemset rows require --allow-special-effects; normal stats unchanged: not enough comparable peer samples; armor unchanged: within +/-15% peer tolerance from 19 sample(s) |
| 16689 | Magister's Mantle | vanilla | 60 | Rare | shoulder | unchanged | itemset detected; set bonuses are not modeled; SQL blocked: special-effect/itemset rows require --allow-special-effects; normal stats unchanged: within +/-15% peer tolerance; armor unchanged: within +/-15% peer tolerance from 27 sample(s) |
| 16695 | Devout Mantle | vanilla | 60 | Rare | shoulder | unchanged | itemset detected; set bonuses are not modeled; SQL blocked: special-effect/itemset rows require --allow-special-effects; normal stats unchanged: within +/-15% peer tolerance; armor unchanged: within +/-15% peer tolerance from 27 sample(s) |
| 16701 | Dreadmist Mantle | vanilla | 60 | Rare | shoulder | unchanged | itemset detected; set bonuses are not modeled; SQL blocked: special-effect/itemset rows require --allow-special-effects; normal stats unchanged: within +/-15% peer tolerance; armor unchanged: within +/-15% peer tolerance from 27 sample(s) |
| 16708 | Shadowcraft Spaulders | vanilla | 60 | Rare | shoulder | unchanged | itemset detected; set bonuses are not modeled; SQL blocked: special-effect/itemset rows require --allow-special-effects; normal stats unchanged: within +/-15% peer tolerance; armor unchanged: within +/-15% peer tolerance from 21 sample(s) |
| 16718 | Wildheart Spaulders | vanilla | 60 | Rare | shoulder | report-only | itemset detected; set bonuses are not modeled; SQL blocked: special-effect/itemset rows require --allow-special-effects; SQL blocked: peer target requires 41.1% visible budget change; visible stat target clamped to +/-20% safety cap; final visible stat budget adjusted to stay within +/-20% safety cap; armor unchanged: within +/-15% peer tolerance from 21 sample(s) |
| 16729 | Lightforge Spaulders | vanilla | 60 | Rare | shoulder | unchanged | itemset detected; set bonuses are not modeled; SQL blocked: special-effect/itemset rows require --allow-special-effects; normal stats unchanged: not enough comparable peer samples; armor unchanged: within +/-15% peer tolerance from 14 sample(s) |
| 16733 | Spaulders of Valor | vanilla | 60 | Rare | shoulder | unchanged | itemset detected; set bonuses are not modeled; SQL blocked: special-effect/itemset rows require --allow-special-effects; normal stats unchanged: within +/-15% peer tolerance; armor unchanged: within +/-15% peer tolerance from 14 sample(s) |
| 20295 | Blue Dragonscale Leggings | vanilla | 60 | Rare | legs | report-only | itemset detected; set bonuses are not modeled; SQL blocked: special-effect/itemset rows require --allow-special-effects; SQL blocked: peer target requires 38.8% visible budget change; visible stat target clamped to +/-20% safety cap; final visible stat budget adjusted to stay within +/-20% safety cap; armor unchanged: within +/-15% peer tolerance from 17 sample(s) |
| 21995 | Boots of Heroism | vanilla | 60 | Epic | feet | unchanged | itemset detected; set bonuses are not modeled; SQL blocked: special-effect/itemset rows require --allow-special-effects; normal stats unchanged: not enough comparable peer samples; armor unchanged: not enough comparable armor samples |
| 21997 | Breastplate of Heroism | vanilla | 60 | Epic | chest | unchanged | itemset detected; set bonuses are not modeled; SQL blocked: special-effect/itemset rows require --allow-special-effects; normal stats unchanged: not enough comparable peer samples; armor unchanged: not enough comparable armor samples |
| 21999 | Helm of Heroism | vanilla | 60 | Epic | head | unchanged | itemset detected; set bonuses are not modeled; SQL blocked: special-effect/itemset rows require --allow-special-effects; normal stats unchanged: not enough comparable peer samples; armor unchanged: not enough comparable armor samples |
| 22003 | Darkmantle Boots | vanilla | 60 | Epic | feet | unchanged | spell payload detected and left unchanged; itemset detected; set bonuses are not modeled; SQL blocked: special-effect/itemset rows require --allow-special-effects; normal stats unchanged: not enough comparable peer samples; armor unchanged: not enough comparable armor samples |
| 22005 | Darkmantle Cap | vanilla | 60 | Epic | head | unchanged | itemset detected; set bonuses are not modeled; SQL blocked: special-effect/itemset rows require --allow-special-effects; normal stats unchanged: not enough comparable peer samples; armor unchanged: not enough comparable armor samples |
| 22009 | Darkmantle Tunic | vanilla | 60 | Epic | chest | unchanged | itemset detected; set bonuses are not modeled; SQL blocked: special-effect/itemset rows require --allow-special-effects; normal stats unchanged: not enough comparable peer samples; armor unchanged: not enough comparable armor samples |
| 22013 | Beastmaster's Cap | vanilla | 60 | Epic | head | unchanged | spell payload detected and left unchanged; itemset detected; set bonuses are not modeled; SQL blocked: special-effect/itemset rows require --allow-special-effects; normal stats unchanged: not enough comparable peer samples; armor unchanged: not enough comparable armor samples |
| 22060 | Beastmaster's Tunic | vanilla | 60 | Epic | chest | unchanged | spell payload detected and left unchanged; itemset detected; set bonuses are not modeled; SQL blocked: special-effect/itemset rows require --allow-special-effects; normal stats unchanged: not enough comparable peer samples; armor unchanged: within +/-15% peer tolerance from 8 sample(s) |
| 22061 | Beastmaster's Boots | vanilla | 60 | Epic | feet | unchanged | spell payload detected and left unchanged; itemset detected; set bonuses are not modeled; SQL blocked: special-effect/itemset rows require --allow-special-effects; normal stats unchanged: not enough comparable peer samples; armor unchanged: not enough comparable armor samples |
| 22064 | Sorcerer's Boots | vanilla | 60 | Epic | feet | unchanged | itemset detected; set bonuses are not modeled; SQL blocked: special-effect/itemset rows require --allow-special-effects; normal stats unchanged: not enough comparable peer samples; armor unchanged: within +/-15% peer tolerance from 8 sample(s) |
| 22065 | Sorcerer's Crown | vanilla | 60 | Epic | head | unchanged | itemset detected; set bonuses are not modeled; SQL blocked: special-effect/itemset rows require --allow-special-effects; normal stats unchanged: not enough comparable peer samples; armor unchanged: within +/-15% peer tolerance from 10 sample(s) |
| 22069 | Sorcerer's Robes | vanilla | 60 | Epic | chest | report-only | spell payload detected and left unchanged; itemset detected; set bonuses are not modeled; SQL blocked: special-effect/itemset rows require --allow-special-effects; final visible stat budget adjusted to stay within +/-20% safety cap; armor unchanged: within +/-15% peer tolerance from 10 sample(s) |
| 22074 | Deathmist Mask | vanilla | 60 | Epic | head | unchanged | itemset detected; set bonuses are not modeled; SQL blocked: special-effect/itemset rows require --allow-special-effects; normal stats unchanged: not enough comparable peer samples; armor unchanged: within +/-15% peer tolerance from 10 sample(s) |
| 22075 | Deathmist Robe | vanilla | 60 | Epic | chest | unchanged | itemset detected; set bonuses are not modeled; SQL blocked: special-effect/itemset rows require --allow-special-effects; normal stats unchanged: not enough comparable peer samples; armor unchanged: within +/-15% peer tolerance from 10 sample(s) |
| 22076 | Deathmist Sandals | vanilla | 60 | Epic | feet | unchanged | itemset detected; set bonuses are not modeled; SQL blocked: special-effect/itemset rows require --allow-special-effects; normal stats unchanged: not enough comparable peer samples; armor unchanged: within +/-15% peer tolerance from 8 sample(s) |
| 22080 | Virtuous Crown | vanilla | 60 | Epic | head | unchanged | itemset detected; set bonuses are not modeled; SQL blocked: special-effect/itemset rows require --allow-special-effects; normal stats unchanged: not enough comparable peer samples; armor unchanged: within +/-15% peer tolerance from 10 sample(s) |
| 22083 | Virtuous Robe | vanilla | 60 | Epic | chest | report-only | itemset detected; set bonuses are not modeled; SQL blocked: special-effect/itemset rows require --allow-special-effects; armor unchanged: within +/-15% peer tolerance from 10 sample(s) |

## Unknown Stat Types

None.

## Calibration Snapshot

Grouped by expansion, item-level band, exact item level, origin, PvP family, quality, slot, inferred role, and peer group. Full data is in the calibration CSV.

| count | expansion_band | ilvl_band | item_level | origin_label | pvp_family | quality_label | slot_key | role | peer_group | median_visible_budget | median_special_weapon_budget | median_total_budget_with_sockets | median_target_budget | median_percent_over_under | peer_outlier_count | sql_blocked_count |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 26 | wrath_t8 | 220-239 | 232 | wrath | none | Epic | weapon_one_hand | physical | origin=2;quality=4;slot=weapon_one_hand;armor=none;weapon=weapon_one_hand:physical;pvp=none;role=physical | 145.485 | 37.0 | 149.16 | 149.16 | 0.0 | 0 | 26 |
| 18 | wrath_t7 | 200-219 | 213 | custom_or_unknown | none | Epic | weapon_two_hand | generic | origin=-1;quality=4;slot=weapon_two_hand;armor=none;weapon=weapon_two_hand:physical;pvp=none;role=generic | 331.36 | 0.0 | 331.36 | 331.36 | 0.0 | 0 | 18 |
| 18 | wrath_t9 | 240-259 | 245 | wrath | none | Epic | weapon_one_hand | physical | origin=2;quality=4;slot=weapon_one_hand;armor=none;weapon=weapon_one_hand:physical;pvp=none;role=physical | 164.86 | 39.0 | 174.86 | 174.86 | 0.0 | 0 | 18 |
| 17 | wrath_t9 | 240-259 | 245 | wrath | none | Epic | weapon_two_hand | physical | origin=2;quality=4;slot=weapon_two_hand;armor=none;weapon=weapon_two_hand:physical;pvp=none;role=physical | 391.12 | 71.0 | 413.85 | 410.485 | 0.82 | 1 | 17 |
| 16 | burning_crusade | 105-119 | 115 | tbc | classic_rank | Rare | weapon_one_hand | generic | origin=1;quality=3;slot=weapon_one_hand;armor=none;weapon=weapon_one_hand:physical;pvp=classic_rank;role=generic | 42.06 | 13.0 | 42.06 | 42.06 | 0.0 | 0 | 16 |
| 16 | burning_crusade | 105-119 | 115 | tbc | none | Rare | weapon_one_hand | physical | origin=1;quality=3;slot=weapon_one_hand;armor=none;weapon=weapon_one_hand:physical;pvp=none;role=physical | 26.39 | 14.5 | 26.39 | 26.39 | 0.814 | 11 | 16 |
| 16 | burning_crusade | 105-119 | 115 | tbc | none | Rare | weapon_two_hand | physical | origin=1;quality=3;slot=weapon_two_hand;armor=none;weapon=weapon_two_hand:physical;pvp=none;role=physical | 63.64 | 35.5 | 64.95 | 64.95 | 0.03 | 12 | 16 |
| 16 | wrath_t8 | 220-239 | 232 | wrath | none | Epic | weapon_two_hand | physical | origin=2;quality=4;slot=weapon_two_hand;armor=none;weapon=weapon_two_hand:physical;pvp=none;role=physical | 348.13 | 74.0 | 357.375 | 357.375 | 0.0 | 1 | 16 |
| 14 | classic_endgame | 060-089 | 75 | custom_or_unknown | none | Epic | weapon_two_hand | physical | origin=-1;quality=4;slot=weapon_two_hand;armor=none;weapon=weapon_two_hand:physical;pvp=none;role=physical | 33.39 | 0.0 | 33.39 | 33.39 | 0.0 | 0 | 14 |
| 14 | wrath_pre_raid | 165-199 | 174 | wrath | none | Uncommon | weapon_one_hand | physical | origin=2;quality=2;slot=weapon_one_hand;armor=none;weapon=weapon_one_hand:physical;pvp=none;role=physical | 44.435 | 16.75 | 44.435 | 44.435 | 0.046 | 12 | 14 |
| 14 | wrath_t8 | 220-239 | 226 | wrath | none | Epic | finger | physical | origin=2;quality=4;slot=finger;armor=0;weapon=none;pvp=none;role=physical | 210.085 | 0.0 | 217.89 | 217.89 | 0.221 | 1 | 14 |
| 12 | burning_crusade | 105-119 | 115 | tbc | none | Rare | chest | caster | origin=1;quality=3;slot=chest;armor=1;weapon=none;pvp=none;role=caster | 85.77 | 0.0 | 107.965 | 107.965 | 0.0 | 2 | 12 |
| 12 | burning_crusade | 105-119 | 115 | tbc | none | Rare | weapon_main_hand | caster | origin=1;quality=3;slot=weapon_main_hand;armor=none;weapon=weapon_main_hand:caster;pvp=none;role=caster | 37.565 | 104.06 | 38.575 | 38.575 | 0.037 | 2 | 12 |
| 12 | burning_crusade | 135-149 | 138 | wrath | none | Uncommon | weapon_one_hand | physical | origin=2;quality=2;slot=weapon_one_hand;armor=none;weapon=weapon_one_hand:physical;pvp=none;role=physical | 36.86 | 8.0 | 36.86 | 36.86 | 0.003 | 5 | 12 |
| 12 | burning_crusade | 135-149 | 146 | wrath | none | Uncommon | weapon_one_hand | physical | origin=2;quality=2;slot=weapon_one_hand;armor=none;weapon=weapon_one_hand:physical;pvp=none;role=physical | 42.0 | 0.0 | 42.0 | 42.0 | 0.0 | 5 | 12 |
| 12 | wrath_t7 | 200-219 | 219 | wrath | none | Epic | weapon_one_hand | physical | origin=2;quality=4;slot=weapon_one_hand;armor=none;weapon=weapon_one_hand:physical;pvp=none;role=physical | 130.12 | 35.0 | 130.12 | 130.12 | 0.005 | 0 | 12 |
| 12 | wrath_t9 | 240-259 | 245 | wrath | none | Epic | chest | tank | origin=2;quality=4;slot=chest;armor=4;weapon=none;pvp=none;role=tank | 452.6 | 0.0 | 484.6 | 484.6 | 0.001 | 0 | 12 |
| 12 | wrath_t9 | 240-259 | 258 | wrath | none | Epic | weapon_two_hand | physical | origin=2;quality=4;slot=weapon_two_hand;armor=none;weapon=weapon_two_hand:physical;pvp=none;role=physical | 446.9 | 94.0 | 478.9 | 478.9 | 0.0 | 0 | 12 |
| 11 | burning_crusade | 105-119 | 115 | tbc | none | Rare | finger | healer | origin=1;quality=3;slot=finger;armor=0;weapon=none;pvp=none;role=healer | 53.3 | 0.0 | 53.3 | 52.175 | 2.156 | 4 | 11 |
| 11 | classic_endgame | 060-089 | 65 | custom_or_unknown | none | Rare | weapon_two_hand | physical | origin=-1;quality=3;slot=weapon_two_hand;armor=none;weapon=weapon_two_hand:physical;pvp=none;role=physical | 41.06 | 0.0 | 41.06 | 41.06 | 0.0 | 0 | 11 |
| 11 | wrath_t10 | 260-284 | 264 | wrath | none | Epic | weapon_one_hand | physical | origin=2;quality=4;slot=weapon_one_hand;armor=none;weapon=weapon_one_hand:physical;pvp=none;role=physical | 188.51 | 46.5 | 204.51 | 201.37 | 1.559 | 1 | 11 |
| 11 | wrath_t9 | 240-259 | 245 | wrath | none | Epic | head | tank | origin=2;quality=4;slot=head;armor=4;weapon=none;pvp=none;role=tank | 440.56 | 0.0 | 480.56 | 484.075 | -0.726 | 0 | 11 |
| 11 | wrath_t9 | 240-259 | 258 | wrath | none | Epic | weapon_one_hand | physical | origin=2;quality=4;slot=weapon_one_hand;armor=none;weapon=weapon_one_hand:physical;pvp=none;role=physical | 190.22 | 42.0 | 206.22 | 203.72 | 1.227 | 0 | 11 |
| 10 | burning_crusade | 105-119 | 115 | tbc | none | Rare | finger | caster | origin=1;quality=3;slot=finger;armor=0;weapon=none;pvp=none;role=caster | 50.675 | 0.0 | 50.675 | 50.675 | 0.51 | 7 | 10 |
| 10 | burning_crusade | 105-119 | 115 | tbc | none | Rare | finger | physical | origin=1;quality=3;slot=finger;armor=0;weapon=none;pvp=none;role=physical | 53.905 | 0.0 | 53.905 | 53.905 | 0.049 | 1 | 10 |
| 10 | burning_crusade | 105-119 | 115 | tbc | none | Rare | head | caster | origin=1;quality=3;slot=head;armor=1;weapon=none;pvp=none;role=caster | 85.13 | 0.0 | 105.13 | 105.13 | 0.007 | 2 | 10 |
| 10 | classic_endgame | 060-089 | 78 | vanilla | classic_rank | Epic | weapon_one_hand | physical | origin=0;quality=4;slot=weapon_one_hand;armor=none;weapon=weapon_one_hand:physical;pvp=classic_rank;role=physical | 18.69 | 14.0 | 18.69 | 18.69 | 0.0 | 0 | 0 |
| 10 | wrath_t7 | 200-219 | 200 | wrath | none | Epic | weapon_one_hand | physical | origin=2;quality=4;slot=weapon_one_hand;armor=none;weapon=weapon_one_hand:physical;pvp=none;role=physical | 97.95 | 38.0 | 97.95 | 97.95 | 0.015 | 2 | 10 |
| 10 | wrath_t7 | 200-219 | 200 | wrath | none | Epic | weapon_two_hand | physical | origin=2;quality=4;slot=weapon_two_hand;armor=none;weapon=weapon_two_hand:physical;pvp=none;role=physical | 234.505 | 65.0 | 234.505 | 234.505 | 0.7 | 7 | 10 |
| 10 | wrath_t7 | 200-219 | 219 | wrath | none | Epic | weapon_two_hand | physical | origin=2;quality=4;slot=weapon_two_hand;armor=none;weapon=weapon_two_hand:physical;pvp=none;role=physical | 313.805 | 34.5 | 321.805 | 321.805 | 0.009 | 0 | 10 |
| 10 | wrath_t8 | 220-239 | 232 | wrath | none | Epic | head | tank | origin=2;quality=4;slot=head;armor=4;weapon=none;pvp=none;role=tank | 389.165 | 0.0 | 429.165 | 429.165 | 0.01 | 0 | 10 |
| 10 | wrath_t9 | 240-259 | 245 | wrath | none | Epic | chest | caster | origin=2;quality=4;slot=chest;armor=1;weapon=none;pvp=none;role=caster | 456.41 | 0.0 | 488.41 | 488.41 | 0.0 | 0 | 10 |
| 10 | wrath_t9 | 240-259 | 245 | wrath | none | Epic | chest | physical | origin=2;quality=4;slot=chest;armor=2;weapon=none;pvp=none;role=physical | 465.62 | 0.0 | 497.62 | 497.62 | 0.0 | 0 | 10 |
| 10 | wrath_t9 | 240-259 | 245 | wrath | none | Epic | finger | physical | origin=2;quality=4;slot=finger;armor=0;weapon=none;pvp=none;role=physical | 255.355 | 0.0 | 259.52 | 259.52 | 0.754 | 2 | 10 |
| 10 | wrath_t9 | 240-259 | 245 | wrath | none | Epic | hands | caster | origin=2;quality=4;slot=hands;armor=1;weapon=none;pvp=none;role=caster | 348.33 | 0.0 | 364.33 | 364.33 | 0.0 | 0 | 10 |
| 10 | wrath_t9 | 240-259 | 245 | wrath | none | Epic | head | physical | origin=2;quality=4;slot=head;armor=3;weapon=none;pvp=none;role=physical | 482.19 | 0.0 | 522.19 | 522.19 | 0.0 | 0 | 10 |
| 10 | wrath_t9 | 240-259 | 245 | wrath | none | Epic | shoulder | tank | origin=2;quality=4;slot=shoulder;armor=4;weapon=none;pvp=none;role=tank | 347.13 | 0.0 | 363.13 | 363.13 | 0.0 | 0 | 10 |
| 9 | burning_crusade | 105-119 | 109 | tbc | none | Rare | finger | generic | origin=1;quality=3;slot=finger;armor=0;weapon=none;pvp=none;role=generic | 18.09 | 0.0 | 18.09 | 29.545 | -38.771 | 9 | 9 |
| 9 | burning_crusade | 105-119 | 115 | tbc | none | Rare | legs | caster | origin=1;quality=3;slot=legs;armor=1;weapon=none;pvp=none;role=caster | 106.68 | 0.0 | 107.78 | 108.39 | -0.563 | 0 | 9 |
| 9 | burning_crusade | 105-119 | 115 | tbc | none | Rare | shoulder | caster | origin=1;quality=3;slot=shoulder;armor=1;weapon=none;pvp=none;role=caster | 69.01 | 0.0 | 85.01 | 84.49 | 0.615 | 1 | 9 |
