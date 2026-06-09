-- The Eye: bind Al'ar shielding cube to the C++ click script.

UPDATE `gameobject_template`
SET `AIName` = '', `ScriptName` = 'go_alar_shielding_cube'
WHERE `entry` = 8181713;
