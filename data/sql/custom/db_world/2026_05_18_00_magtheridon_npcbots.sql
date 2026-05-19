-- Magtheridon's Lair: bind the localized C++ encounter rewrite with NPCBot cube support.

UPDATE `creature_template`
SET `AIName` = '', `ScriptName` = 'boss_magtheridon'
WHERE `entry` = 17257;

UPDATE `creature_template`
SET `AIName` = '', `ScriptName` = 'npc_hellfire_channeler_magtheridon'
WHERE `entry` = 17256;

DELETE FROM `smart_scripts`
WHERE `entryorguid` = 17256
  AND `source_type` = 0;

UPDATE `gameobject_template`
SET `ScriptName` = 'go_manticron_cube'
WHERE `entry` = 181713;

DELETE FROM `spell_script_names`
WHERE `ScriptName` IN
(
    'spell_magtheridon_blaze',
    'spell_magtheridon_shadow_grasp',
    'spell_magtheridon_shadow_grasp_visual',
    'spell_magtheridon_blast_nova',
    'spell_magtheridon_quake',
    'spell_magtheridon_debris_target_selector'
);

INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES
(30541, 'spell_magtheridon_blaze'),
(30410, 'spell_magtheridon_shadow_grasp'),
(30166, 'spell_magtheridon_shadow_grasp_visual'),
(30616, 'spell_magtheridon_blast_nova'),
(30658, 'spell_magtheridon_quake'),
(30629, 'spell_magtheridon_debris_target_selector');
