-- Attach the configurable level 70 gate to Burning Crusade leatherworking drums.
-- The C++ config defaults to disabled, so these rows are inert until enabled.

DELETE FROM `spell_script_names`
WHERE `ScriptName` = 'spell_custom_disable_above_level_70'
  AND `spell_id` IN (35474, 35475, 35476, 35477, 35478);

INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES
(35474, 'spell_custom_disable_above_level_70'), -- Drums of Panic
(35475, 'spell_custom_disable_above_level_70'), -- Drums of War
(35476, 'spell_custom_disable_above_level_70'), -- Drums of Battle
(35477, 'spell_custom_disable_above_level_70'), -- Drums of Speed
(35478, 'spell_custom_disable_above_level_70'); -- Drums of Restoration
