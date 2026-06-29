-- Sunwell Plateau: bind Brutallus Meteor Slash to its configurable damage multiplier script.

DELETE FROM `spell_script_names`
WHERE `spell_id` = 45150
  AND `ScriptName` = 'spell_brutallus_meteor_slash';

INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES
(45150, 'spell_brutallus_meteor_slash');
