-- Sunwell Plateau: add Brutallus-specific Burn cap, tank-strip, no-refresh spread, and NPCBot hold handling.

DELETE FROM `spell_script_names`
WHERE `spell_id` IN (45141, 45151)
  AND `ScriptName` = 'spell_brutallus_burn';

DELETE FROM `spell_script_names`
WHERE `spell_id` = 46394
  AND `ScriptName` = 'spell_brutallus_burn_damage';

INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES
(45141, 'spell_brutallus_burn'),
(45151, 'spell_brutallus_burn'),
(46394, 'spell_brutallus_burn_damage');
