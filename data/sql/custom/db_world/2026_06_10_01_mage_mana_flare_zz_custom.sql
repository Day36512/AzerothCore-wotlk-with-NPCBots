-- Mage: bind custom Pyroblast Mana Flare burn and expiry detonation scripts.

DELETE FROM `spell_script_names`
WHERE `ScriptName` IN (
  'spell_mage_custom_pyroblast',
  'spell_mage_mana_flare'
);

INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES
(-11366, 'spell_mage_custom_pyroblast'), -- Pyroblast, all ranks
(300273, 'spell_mage_mana_flare');       -- Mana Flare burn aura
