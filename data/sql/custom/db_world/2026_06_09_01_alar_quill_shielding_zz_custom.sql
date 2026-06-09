-- The Eye: reduce Al'ar Flame Quills missile damage for shielding cube protection aura.

DELETE FROM `spell_script_names`
WHERE `ScriptName` = 'spell_alar_quill_missile'
  AND `spell_id` IN (
    34269, 34270, 34271, 34272, 34273, 34274, 34275, 34276, 34277, 34278, 34279,
    34280, 34281, 34282, 34283, 34284, 34285, 34286, 34287, 34288, 34289,
    34314, 34315, 34316
  );

INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES
(34269, 'spell_alar_quill_missile'),
(34270, 'spell_alar_quill_missile'),
(34271, 'spell_alar_quill_missile'),
(34272, 'spell_alar_quill_missile'),
(34273, 'spell_alar_quill_missile'),
(34274, 'spell_alar_quill_missile'),
(34275, 'spell_alar_quill_missile'),
(34276, 'spell_alar_quill_missile'),
(34277, 'spell_alar_quill_missile'),
(34278, 'spell_alar_quill_missile'),
(34279, 'spell_alar_quill_missile'),
(34280, 'spell_alar_quill_missile'),
(34281, 'spell_alar_quill_missile'),
(34282, 'spell_alar_quill_missile'),
(34283, 'spell_alar_quill_missile'),
(34284, 'spell_alar_quill_missile'),
(34285, 'spell_alar_quill_missile'),
(34286, 'spell_alar_quill_missile'),
(34287, 'spell_alar_quill_missile'),
(34288, 'spell_alar_quill_missile'),
(34289, 'spell_alar_quill_missile'),
(34314, 'spell_alar_quill_missile'),
(34315, 'spell_alar_quill_missile'),
(34316, 'spell_alar_quill_missile');
