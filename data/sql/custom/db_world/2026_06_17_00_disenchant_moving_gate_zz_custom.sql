-- Attach the configurable moving-cast gate to Disenchant.
-- The C++ config defaults to disabled, so this row is inert until enabled.

DELETE FROM `spell_script_names`
WHERE `spell_id` = 13262
  AND `ScriptName` = 'spell_custom_disenchant_disable_moving';

INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES
(13262, 'spell_custom_disenchant_disable_moving'); -- Disenchant
