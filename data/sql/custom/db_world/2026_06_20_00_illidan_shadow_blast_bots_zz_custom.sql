DELETE FROM `spell_script_names`
WHERE `spell_id` = 41078
  AND `ScriptName` = 'spell_illidan_shadow_blast';

DELETE FROM `spell_script_names`
WHERE `spell_id` = 41131
  AND `ScriptName` = 'spell_illidan_shadow_blast';

INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES
(41078, 'spell_illidan_shadow_blast'),
(41131, 'spell_illidan_shadow_blast');
