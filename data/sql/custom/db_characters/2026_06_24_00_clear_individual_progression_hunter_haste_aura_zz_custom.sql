-- Remove the retired IndividualProgression hunter ranged haste adjustment aura from saved characters.
DELETE FROM `character_aura` WHERE `spell` = 89507;
DELETE FROM `pet_aura` WHERE `spell` = 89507;
