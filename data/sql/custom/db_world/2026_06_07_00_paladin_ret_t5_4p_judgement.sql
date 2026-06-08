-- Ret T5 4P: 600403 is only the passive marker aura.
-- The bonus is handled from the player Judgement casts.

DELETE FROM `spell_script_names`
WHERE `ScriptName` = 'spell_custom_pal_t5_ret_4p';

INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES
(53407, 'spell_custom_pal_t5_ret_4p'), -- Judgement of Justice
(20271, 'spell_custom_pal_t5_ret_4p'), -- Judgement of Light
(53408, 'spell_custom_pal_t5_ret_4p'); -- Judgement of Wisdom
