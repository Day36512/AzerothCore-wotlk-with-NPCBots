-- Sunwell Plateau: allow Felmyst Encapsulate damage to be configured.
DELETE FROM `spell_script_names`
WHERE `spell_id` = 45662
  AND `ScriptName` = 'spell_felmyst_encapsulate_damage';

INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES
(45662, 'spell_felmyst_encapsulate_damage');
