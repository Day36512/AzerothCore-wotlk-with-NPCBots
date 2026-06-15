-- Hyjal Summit: bind Archimonde Air Burst post-hit NPCBot fall-protection support.

DELETE FROM `spell_script_names`
WHERE `spell_id` = 32014
  AND `ScriptName` = 'spell_archimonde_air_burst_fall_protection';

INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES
(32014, 'spell_archimonde_air_burst_fall_protection');
