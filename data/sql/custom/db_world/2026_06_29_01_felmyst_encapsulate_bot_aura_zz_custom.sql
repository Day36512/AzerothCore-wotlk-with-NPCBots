-- Sunwell Plateau: let Felmyst Encapsulate lift NPCBots while preserving the stock linked spell.
DELETE FROM `spell_script_names`
WHERE `spell_id` = 45665
  AND `ScriptName` = 'spell_felmyst_encapsulate_bot_aura';

INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES
(45665, 'spell_felmyst_encapsulate_bot_aura');
