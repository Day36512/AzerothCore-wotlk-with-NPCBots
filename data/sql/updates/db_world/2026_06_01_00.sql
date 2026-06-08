-- DB update 2026_05_31_01 -> 2026_06_01_00
--
-- Savage weapon flags: no-disenchant and ability to sell them back to vendors for refund
UPDATE `item_template` SET `Flags` = `Flags`|4096|32768 WHERE `entry` IN (42356,42511,42557,42221,42295,42212,42206,42294,42523,42382,44416,42344,42297,42535,42213,42446,42574,42575,42576,42219,42220,42611,42612,42445,42215,42296,42517,42222,42223,42568,42529,42218,42447,42556,42216,42217,42618,42224,42388,42343,42448,42593,42594,42595,42444,44415,42214);

DELETE FROM `spell_script_names` WHERE `spell_id` = 38235 AND `ScriptName` = 'spell_hydross_water_tomb';
INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES (38235, 'spell_hydross_water_tomb');

DELETE FROM `spell_script_names` WHERE `spell_id` = 38280 AND `ScriptName` = 'spell_lady_vashj_static_charge_bot_movement';
INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES (38280, 'spell_lady_vashj_static_charge_bot_movement');
