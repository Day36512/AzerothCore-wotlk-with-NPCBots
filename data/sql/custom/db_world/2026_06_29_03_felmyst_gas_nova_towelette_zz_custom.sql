-- Sunwell Plateau: let Felmyst Gas Nova give NPCBots a small Moist Towelette reaction chance.
DELETE FROM `spell_script_names`
WHERE `spell_id` = 45855
  AND `ScriptName` = 'spell_felmyst_gas_nova_bot_towelette';

INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES
(45855, 'spell_felmyst_gas_nova_bot_towelette');
