-- Sunwell Plateau: split Kalecgos Boundless Agony spell scripts by DBC target type.
-- 45032 is the enemy-target boss cast; 45034 is the ally-target player bounce/aura.

DELETE FROM `spell_script_names`
WHERE `spell_id` IN (45032, 45034)
  AND `ScriptName` IN (
    'spell_kalecgos_curse_of_boundless_agony',
    'spell_kalecgos_curse_of_boundless_agony_aura',
    'spell_kalecgos_curse_of_boundless_agony_player'
  );

INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES
(45032, 'spell_kalecgos_curse_of_boundless_agony'),
(45034, 'spell_kalecgos_curse_of_boundless_agony_player');
