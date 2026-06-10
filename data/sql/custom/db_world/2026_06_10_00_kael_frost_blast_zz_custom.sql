-- The Eye: bind Kael'thas custom Frost Blast to local target filtering and capped periodic damage.

DELETE FROM `spell_script_names`
WHERE `spell_id` = 600449
  AND `ScriptName` IN (
    'spell_kaelthas_custom_frost_blast',
    'spell_kaelthas_custom_frost_blast_aura'
  );

INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES
(600449, 'spell_kaelthas_custom_frost_blast'),
(600449, 'spell_kaelthas_custom_frost_blast_aura');
