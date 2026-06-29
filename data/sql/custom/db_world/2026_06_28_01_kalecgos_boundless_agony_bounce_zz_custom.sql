-- Sunwell Plateau: bind the player Curse of Boundless Agony bounce to its ally target selector.

DELETE FROM `spell_script_names`
WHERE `spell_id` = 45034
  AND `ScriptName` IN (
    'spell_kalecgos_curse_of_boundless_agony_aura',
    'spell_kalecgos_curse_of_boundless_agony_player'
  );

INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES
(45034, 'spell_kalecgos_curse_of_boundless_agony_player');
