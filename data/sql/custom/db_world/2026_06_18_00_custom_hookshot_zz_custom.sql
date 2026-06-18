-- Battle for Mount Hyjal: bind the custom gargoyle hookshot spell script.

DELETE FROM `spell_script_names`
WHERE `spell_id` = 600453
  AND `ScriptName` = 'custom_hookshot';

INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES
(600453, 'custom_hookshot');
