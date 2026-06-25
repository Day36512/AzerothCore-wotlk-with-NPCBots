-- Quivers and ammo pouches should not add restored pre-Wrath haste spells.
UPDATE `item_template`
SET `spellid_1` = 0,
    `spelltrigger_1` = 0,
    `spellcharges_1` = 0,
    `spellppmRate_1` = 0,
    `spellcooldown_1` = -1,
    `spellcategory_1` = 0,
    `spellcategorycooldown_1` = -1
WHERE `class` = 11 AND `subclass` IN (2, 3);

-- Remove the old IndividualProgression hunter haste adjustment spell if it was already loaded.
DELETE FROM `spell_dbc` WHERE `ID` = 89507;
